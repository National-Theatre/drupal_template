<?php
/**
 * @copyright The Royal National Theatre
 * @author John-Paul Drawneek <jdrawneek@nationaltheatre.org.uk>
 */

function drupal_move_uploaded_file_hack($filename, $uri) {
  $result = @copy($filename, $uri);
  // PHP's move_uploaded_file() does not properly support streams if safe_mode
  // or open_basedir are enabled so if the move failed, try finding a real path
  // and retry the move operation.
  if (!$result) {
    if ($realpath = drupal_realpath($uri)) {
      $result = copy($filename, $realpath);
    }
    else {
      $result = copy($filename, $uri);
    }
  }

  return $result;
}

function file_save_upload_hack($source, $validators = array(), $destination = FALSE, $replace = FILE_EXISTS_RENAME) {
  global $user;
  static $upload_cache;

  // Return cached objects without processing since the file will have
  // already been processed and the paths in _FILES will be invalid.
  if (isset($upload_cache[$source])) {
    //return $upload_cache[$source];
  }

  // Make sure there's an upload to process.
  if (empty($_FILES['files']['name'][$source])) {
    return NULL;
  }

  // Check for file upload errors and return FALSE if a lower level system
  // error occurred. For a complete list of errors:
  // See http://php.net/manual/en/features.file-upload.errors.php.

  switch ($_FILES['files']['error'][$source]) {
    case UPLOAD_ERR_INI_SIZE:
    case UPLOAD_ERR_FORM_SIZE:
      drupal_set_message(t('The file %file could not be saved, because it exceeds %maxsize, the maximum allowed size for uploads.', array('%file' => $_FILES['files']['name'][$source], '%maxsize' => format_size(file_upload_max_size()))), 'error');
      return FALSE;

    case UPLOAD_ERR_PARTIAL:
    case UPLOAD_ERR_NO_FILE:
      drupal_set_message(t('The file %file could not be saved, because the upload did not complete.', array('%file' => $_FILES['files']['name'][$source])), 'error');
      return FALSE;

    case UPLOAD_ERR_OK:
      // Final check that this is a valid upload, if it isn't, use the
      // default error handler.
//            if (is_uploaded_file($_FILES['files']['tmp_name'][$source])) {
        break;
//            }

    // Unknown error
    default:
      drupal_set_message(t('The file %file could not be saved. An unknown error has occurred.', array('%file' => $_FILES['files']['name'][$source])), 'error');
      return FALSE;
  }

  // Begin building file object.
  $file = new stdClass();
  $file->uid      = $user->uid;
  $file->status   = 0;
  $file->filename = trim(drupal_basename($_FILES['files']['name'][$source]), '.');
  $file->uri      = $_FILES['files']['tmp_name'][$source];
  $file->filemime = file_get_mimetype($file->filename);
  $file->filesize = $_FILES['files']['size'][$source];

  $extensions = '';
  if (isset($validators['file_validate_extensions'])) {
    if (isset($validators['file_validate_extensions'][0])) {
      // Build the list of non-munged extensions if the caller provided them.
      $extensions = $validators['file_validate_extensions'][0];
    }
    else {
      // If 'file_validate_extensions' is set and the list is empty then the
      // caller wants to allow any extension. In this case we have to remove the
      // validator or else it will reject all extensions.
      unset($validators['file_validate_extensions']);
    }
  }
  else {
    // No validator was provided, so add one using the default list.
    // Build a default non-munged safe list for file_munge_filename().
    $extensions = 'jpg jpeg gif png txt doc xls pdf ppt pps odt ods odp';
    $validators['file_validate_extensions'] = array();
    $validators['file_validate_extensions'][0] = $extensions;
  }

  if (!empty($extensions)) {
    // Munge the filename to protect against possible malicious extension hiding
    // within an unknown file type (ie: filename.html.foo).
    $file->filename = file_munge_filename($file->filename, $extensions);
  }

  // Rename potentially executable files, to help prevent exploits (i.e. will
  // rename filename.php.foo and filename.php to filename.php.foo.txt and
  // filename.php.txt, respectively). Don't rename if 'allow_insecure_uploads'
  // evaluates to TRUE.
  if (!variable_get('allow_insecure_uploads', 0) && preg_match('/\.(php|pl|py|cgi|asp|js)(\.|$)/i', $file->filename) && (substr($file->filename, -4) != '.txt')) {
    $file->filemime = 'text/plain';
    $file->uri .= '.txt';
    $file->filename .= '.txt';
    // The .txt extension may not be in the allowed list of extensions. We have
    // to add it here or else the file upload will fail.
    if (!empty($extensions)) {
      $validators['file_validate_extensions'][0] .= ' txt';
      drupal_set_message(t('For security reasons, your upload has been renamed to %filename.', array('%filename' => $file->filename)));
    }
  }

  // If the destination is not provided, use the temporary directory.
  if (empty($destination)) {
    $destination = 'temporary://';
  }

  // Assert that the destination contains a valid stream.
  $destination_scheme = file_uri_scheme($destination);
  if (!$destination_scheme || !file_stream_wrapper_valid_scheme($destination_scheme)) {
    drupal_set_message(t('The file could not be uploaded, because the destination %destination is invalid.', array('%destination' => $destination)), 'error');
    return FALSE;
  }

  $file->source = $source;
  // A URI may already have a trailing slash or look like "public://".
  if (substr($destination, -1) != '/') {
    $destination .= '/';
  }
  $file->destination = file_destination($destination . $file->filename, $replace);
  // If file_destination() returns FALSE then $replace == FILE_EXISTS_ERROR and
  // there's an existing file so we need to bail.
  if ($file->destination === FALSE) {
    drupal_set_message(t('The file %source could not be uploaded because a file by that name already exists in the destination %directory.', array('%source' => $source, '%directory' => $destination)), 'error');
    return FALSE;
  }

  // Add in our check of the the file name length.
  $validators['file_validate_name_length'] = array();

  // Call the validation functions specified by this function's caller.
  $errors = file_validate($file, $validators);

  // Check for errors.
  if (!empty($errors)) {
    $message = t('The specified file %name could not be uploaded.', array('%name' => $file->filename));
    if (count($errors) > 1) {
      $message .= theme('item_list', array('items' => $errors));
    }
    else {
      $message .= ' ' . array_pop($errors);
    }
    form_set_error($source, $message);
    return FALSE;
  }

  // Move uploaded files from PHP's upload_tmp_dir to Drupal's temporary
  // directory. This overcomes open_basedir restrictions for future file
  // operations.
  $file->uri = $file->destination;
  if (!drupal_move_uploaded_file_hack($_FILES['files']['tmp_name'][$source], $file->uri)) {
    form_set_error($source, t('File upload error. Could not move uploaded file.'));
    watchdog('file', 'Upload error. Could not move uploaded file %file to destination %destination.', array('%file' => $file->filename, '%destination' => $file->uri));
    return FALSE;
  }

  // Set the permissions on the new file.
  drupal_chmod($file->uri);

  // If we are replacing an existing file re-use its database record.
  if ($replace == FILE_EXISTS_REPLACE) {
    $existing_files = file_load_multiple(array(), array('uri' => $file->uri));
    if (count($existing_files)) {
      $existing = reset($existing_files);
      $file->fid = $existing->fid;
    }
  }

  // If we made it this far it's safe to record this file in the database.
  if ($file = file_save($file)) {
    // Add file to the cache.
    $upload_cache[$source] = $file;
    return $file;
  }
  return FALSE;
}
