<?php
namespace NT\Drupal\Testing\PHPUnit;
require_once realpath(dirname(__FILE__) . '/../') . '/vendor/autoload.php';
require_once realpath(dirname(__FILE__) . '/') . '/drupal_hack.inc';

// This code is in global scope.
// TODO: I would rather this code at top of file, but I get Fatal error: Class 'Drush_TestCase' not found

upal::init();
