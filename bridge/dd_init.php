<?php

namespace DDTrace\Bridge;

require_once __DIR__ . '/dd_autoload.php';

use DDTrace\Encoders\Json;
use DDTrace\GlobalTracer;
use DDTrace\Integrations\IntegrationsLoader;
use DDTrace\Tracer;
use DDTrace\Transport\Http;

$tracer = new Tracer(new Http(new Json()));

GlobalTracer::set($tracer);

$rootScope = $tracer->startActiveSpan('main.span.created.by.ext');

register_shutdown_function(function() use ($rootScope, $tracer) {
    $rootScope->close();
    $tracer->flush();
});

IntegrationsLoader::load();
