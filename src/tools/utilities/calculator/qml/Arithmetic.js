.pragma library

// Arithmetic only. A separate name avoids colliding with the Calculator QML type.
// Expressions never run through JavaScript eval or Function.
function calculate(source) {
    if (typeof source !== "string" || source.length > 256) throw new Error("Expression is too long.")
    var text=source.trim(), position=0, depth=0
    if (!text.length) throw new Error("Enter a calculation.")
    function skipSpaces() { while (position<text.length && /\s/.test(text[position])) ++position }
    function checked(value) {
        if (!isFinite(value)) throw new Error("Result is outside the supported range.")
        return value
    }
    function primary() {
        if (++depth>32) throw new Error("Too many nested operations.")
        skipSpaces()
        var value, sign=text[position]
        if (sign === "+" || sign === "-") { ++position; value=(sign === "-" ? -1 : 1)*primary() }
        else if (sign === "(") {
            ++position; value=sum()
            if (text[position++] !== ")") throw new Error("Close the parentheses.")
        } else {
            var match=/^(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?/.exec(text.slice(position))
            if (!match) throw new Error("Enter a number or parentheses.")
            position+=match[0].length; value=Number(match[0])
        }
        skipSpaces()
        while (text[position] === "%") { value/=100; ++position; skipSpaces() }
        --depth; return checked(value)
    }
    function product() {
        var value=primary()
        while (text[position] === "*" || text[position] === "/") {
            var op=text[position++], operand=primary()
            if (op === "/" && operand === 0) throw new Error("Cannot divide by zero.")
            value=checked(op === "*" ? value*operand : value/operand)
        }
        return value
    }
    function sum() {
        var value=product()
        while (text[position] === "+" || text[position] === "-") {
            var op=text[position++], operand=product()
            value=checked(op === "+" ? value+operand : value-operand)
        }
        return value
    }
    var result=sum()
    if (position !== text.length) throw new Error("Unsupported character or missing operator.")
    return Number(result.toPrecision(12)).toString()
}
