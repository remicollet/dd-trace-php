<?php

/**
 * Ported from opentracing/opentracing
 * @see https://github.com/opentracing/opentracing-php/blob/master/src/OpenTracing/Exceptions/InvalidReferenceArgument.php
 */

namespace DDTrace\OpenTracing\Exceptions;

use InvalidArgumentException;

/**
 * Thrown when passing an invalid argument for a reference
 */
final class InvalidReferenceArgument extends InvalidArgumentException
{
    /**
     * @return InvalidReferenceArgument
     */
    public static function forEmptyType()
    {
        return new self('Reference type can not be an empty string');
    }

    /**
     * @param mixed $context
     * @return InvalidReferenceArgument
     */
    public static function forInvalidContext($context)
    {
        return new self(sprintf(
            'Reference expects \DDTrace\OpenTracing\Span or \DDTrace\OpenTracing\SpanContext as context, got %s',
            is_object($context) ? get_class($context) : gettype($context)
        ));
    }
}