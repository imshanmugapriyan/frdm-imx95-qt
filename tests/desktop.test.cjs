const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const context = vm.createContext({});
vm.runInContext(fs.readFileSync(path.join(__dirname, '../src/tools/utilities/calculator/qml/Arithmetic.js'), 'utf8')
    .replace(/^\.pragma library\s*/, ''), context);
const calculate = context.calculate;
let checks = 0;
function test(name, action) { action(); ++checks; process.stdout.write(`PASS ${name}\n`); }

test('multiplication and division precede addition and subtraction', () => {
    assert.equal(calculate('2+3*4'), '14');
    assert.equal(calculate('20 / 2 / 5 + 1'), '3');
    assert.equal(calculate('10-3-2'), '5');
});
test('parentheses and unary signs compose correctly', () => {
    assert.equal(calculate('-(2+3)*4'), '-20');
    assert.equal(calculate('1--2'), '3');
    assert.equal(calculate('(-(-3))'), '3');
    assert.equal(calculate(' - ( 2 + 3 ) '), '-5');
});
test('decimal arithmetic presents useful precision', () => {
    assert.equal(calculate('0.1+0.2'), '0.3');
    assert.equal(calculate('.5*2.'), '1');
    assert.equal(calculate('1/3'), '0.333333333333');
    assert.equal(calculate('-0'), '0');
});
test('percent is an explicit operand divided by 100', () => {
    assert.equal(calculate('200*10%'), '20');
    assert.equal(calculate('-20%'), '-0.2');
    assert.equal(calculate('(25+25)%'), '0.5');
    assert.equal(calculate('200+10%'), '200.1');
});
test('scientific results can be used in subsequent calculations', () => {
    const result = calculate('1e100*10');
    assert.equal(calculate(result+'/1e100'), '10');
    assert.equal(calculate('1e-3 + 2e-3'), '0.003');
});
test('zero division and overflow report errors', () => {
    assert.throws(() => calculate('1/(2-2)'), /zero/);
    assert.throws(() => calculate('1/-0'), /zero/);
    assert.throws(() => calculate('1e309'), /range/);
    assert.throws(() => calculate('1e308*10'), /range/);
});
test('missing operators, malformed parentheses and incomplete inputs are rejected', () => {
    for (const expression of ['', ' ', '()', '2(3)', '(1', '1)', '1+', '2**3', '1..2', '12 34', '1 e2'])
        assert.throws(() => calculate(expression), expression);
});
test('expressions cannot execute JavaScript or access object properties', () => {
    for (const expression of ['Math.random()', 'globalThis', '1;process.exit()', 'a=2', '[1][0]', '1.toString()'])
        assert.throws(() => calculate(expression), expression);
});
test('length and nesting limits bound user input', () => {
    assert.throws(() => calculate('1'.repeat(257)), /long/);
    assert.throws(() => calculate('('.repeat(40)+'1'+')'.repeat(40)), /nested/);
    assert.throws(() => calculate(null));
});
process.stdout.write(`${checks} desktop arithmetic checks passed; no Qt runtime used.\n`);
