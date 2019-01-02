<?php

namespace DDTrace\Integrations\Symfony\V3;

class SymfonyIntegration
{
    public static function load()
    {
        dd_trace('Illuminate\Foundation\ProviderRepository', 'load', function(array $providers) {
            return $this->load($app, array_merge($providers, ['\DDTrace\Integrations\Symfony\V3\SymfonyBundle']));
        });
    }
}
