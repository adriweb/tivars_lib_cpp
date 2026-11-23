# TI-83 Evo exact math

The TI-83 Evo can simplify and preserve exact expressions built from numbers
and constants. On the home screen, variables are substituted with their
stored values before arithmetic. For example, `X+X` returns `6` when
`X = 3`. These results describe exact numeric arithmetic with symbolic
forms for selected constants and operations.

The examples below were checked with the TI-83 Evo OS in an emulator, using
AUTO answers, radians, and MathPrint. Complex examples use `a+bi` mode.
The [scalar file reference](8xn2-number.md) gives the corresponding transfer
format for captured exports.

## Results checked on OS 7.1.0.4421

| Input | Result |
| --- | --- |
| `sqrt(8)` | `2*sqrt(2)` |
| `sqrt(2)+sqrt(3)` | `sqrt(3)+sqrt(2)` |
| `sqrt(-8)` | `2*sqrt(2)*i` |
| `1/3+sqrt(2)*i`, with a fraction template | Both components remain exact |
| `(2/3)i`, with a fraction template | The imaginary coefficient stays exact |
| `ln(2)` | Retained as `ln(2)` |
| `2^(1/3)`, with a fraction template | Retained as `2^(1/3)` |
| `pi`, `e^1` | Stored as `pi` and `e` |
| `-1.5479162+(2/3)i` | The decimal real term makes the imaginary coefficient approximate |

The exact results above survived assignment to a variable and export as
`.8xn2`. The paired 7.0 and 7.1 exports of `pi`, `2*sqrt(2)`, and
template-entered `1/3` also store identical expression words.

## Additional results checked on OS 7.0.0.3996

| Family | Examples |
| --- | --- |
| Fractions | Template-entered `1/2 + 1/3` returns `5/6`. |
| Radicals | `sqrt(1/8)` returns `sqrt(2)/4`. `sqrt(2)+sqrt(8)` returns `3*sqrt(2)`. |
| Rationalization | `1/sqrt(2)` returns `sqrt(2)/2`. |
| Nested radicals | `sqrt(2+sqrt(3))` returns `(sqrt(6)+sqrt(2))/2`. |
| Powers | `8^(2/3)` returns `4`. `2^sqrt(2)` and `pi^2` remain exact expressions. |
| Constants | `pi+pi` returns `2*pi`. `e+e` returns `2*e`. |
| Logarithms | `ln(2)+ln(3)` returns `ln(6)`. `ln(4)` returns `2*ln(2)`. `log(2)` stays exact. |
| Trigonometry | `sin(pi/6)` returns `1/2`. `asin(1/2)` returns `pi/6`. `sin(pi/7)` remains exact. |
| Complex values | `sqrt(-108)` returns `6*sqrt(3)*i`. |
| Factorial | `10!` returns `3628800`. |

With the exact fraction template and `a+bi` mode,
`(-6+sqrt(6^2-4*9*4))/(2*9)` returns `-1/3+(sqrt(3)/3)i`.

A stored `sqrt(2)+sqrt(3)` survived leaving and reopening Calculator.
A list containing exact `1/3` and `sqrt(2)` retained both values on screen,
and multiplying the list by `3` returned `{1,3*sqrt(2)}`. Those list
observations describe on-calculator arithmetic. The corresponding composite
list and matrix transfer formats still need captured examples.

## Input and display affect exactness

On both versions, the stacked fraction template retains `1/3`. Ordinary
division follows a decimal path in AUTO mode. For example, entering
`1/3+1/6` with the divide key returns `0.5`. The exact/decimal conversion
key then displays the result as `1/2`.

On OS 7.0, irreducible template fractions with a numerator or denominator
up to `9,999,999,999` stayed exact in the tested cases. The tested
`10000000000/3` and `1/10000000000` displayed decimals. Separately, a square
root with a 20-digit radicand and a sum of five unlike radical terms stayed
exact. These examples describe observed display behavior. The fraction
cutoff should only be applied to the tested input path.
