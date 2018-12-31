<?php

$dd_autoload_called = false;

dd_trace('spl_autoload_register', function($callback) use (&$dd_autoload_called) {
    $originalAutoloaderRegistered = spl_autoload_register($callback);
    if (!$dd_autoload_called
            && is_array($callback)
            && is_object($callback[0])
            && get_class($callback[0]) === 'Composer\Autoload\ClassLoader'
    ) {
        $dd_autoload_called = true;
        require_once __DIR__ . '/dd_init.php';
    }
    return $originalAutoloaderRegistered;
});
