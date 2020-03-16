<?php

namespace DDTrace\Tests\Integrations\CLI\Laravel\V5_8;

use DDTrace\Tests\Common\SpanAssertion;
use DDTrace\Tests\Integrations\CLI\CLITestCase;

class CommonScenariosTest extends CLITestCase
{
    const IS_SANDBOX = false;

    protected function getScriptLocation()
    {
        return __DIR__ . '/../../../../Frameworks/Laravel/Version_5_8/artisan';
    }

    protected static function getEnvs()
    {
        return array_merge(parent::getEnvs(), [
            'APP_NAME' => 'artisan_test_app',
        ]);
    }

    public function testCommandWithNoArguments()
    {
        $traces = $this->getTracesFromCommand();

        $this->assertFlameGraph($traces, [
            SpanAssertion::build(
                'laravel.artisan',
                'artisan_test_app',
                'cli',
                'artisan'
            )->withExactTags([
                'integration.name' => 'laravel',
            ])->withChildren([
                SpanAssertion::exists(
                    'laravel.provider.load',
                    'Illuminate\Foundation\ProviderRepository::load'
                )->onlyIf(static::IS_SANDBOX),
            ]),
        ]);
    }

    public function testCommandWithArgument()
    {
        $traces = $this->getTracesFromCommand('route:list');

        $this->assertFlameGraph($traces, [
            SpanAssertion::build(
                'laravel.artisan',
                'artisan_test_app',
                'cli',
                'artisan route:list'
            )->withExactTags([
                'integration.name' => 'laravel',
            ])->withChildren([
                SpanAssertion::exists(
                    'laravel.provider.load',
                    'Illuminate\Foundation\ProviderRepository::load'
                )->onlyIf(static::IS_SANDBOX),
            ]),
        ]);
    }

    public function testCommandWithError()
    {
        $traces = $this->getTracesFromCommand('foo:error');

        $this->assertFlameGraph($traces, [
            SpanAssertion::build(
                'laravel.artisan',
                'artisan_test_app',
                'cli',
                'artisan foo:error'
            )->withExactTags([
                'integration.name' => 'laravel',
            ])->withExistingTagsNames([
                'error.msg',
                'error.stack'
            ])->withChildren([
                SpanAssertion::exists(
                    'laravel.provider.load',
                    'Illuminate\Foundation\ProviderRepository::load'
                )->onlyIf(static::IS_SANDBOX),
            ])->setError(),
        ]);
    }
}
