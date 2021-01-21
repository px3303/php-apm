<?php
namespace apm;

function bar() {
    date('Y-m-d');
    round(234.6);
}

function foo() {
    bar();
    uniqid();
}

foo();
