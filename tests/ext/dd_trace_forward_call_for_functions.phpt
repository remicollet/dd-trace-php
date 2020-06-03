--TEST--
The original function call is invoked from the closure
--SKIPIF--
<?php if (PHP_MAJOR_VERSION > 5) die('skip: test requires legacy API'); ?>
--FILE--
<?php
function doStuff($foo, array $bar = [])
{
    return '[' . $foo . '] ' . array_sum($bar);
}

// Cannot call a function while it is not traced and later expect it to trace
//echo doStuff('Before', [1, 2]) . "\n";

dd_trace('doStuff', function () {
    echo "**TRACED**\n";
    return dd_trace_forward_call();
});
dd_trace('array_sum', function () {
    echo "**TRACED INTERNAL**\n";
    return dd_trace_forward_call();
});

echo doStuff('After', [2, 3]) . "\n";
?>
--EXPECT--
**TRACED**
**TRACED INTERNAL**
[After] 5
