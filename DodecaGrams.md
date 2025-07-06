**DGs** (Dodecagrams/Base-12 values) are a signature feature of QuarterLang. You can write, convert, and use them directly in your code—making base-12 arithmetic and representation a first-class part of your programs.

Here’s how you can **write and use DGs in QuarterLang** (based on your project’s docs):

---

## **1. Declaring a DG Value**

```quarter
val dozen as dg: XY     # Declares a base-12 value with DG literal 'XY'
```

* `val` — immutable variable.
* `dozen` — variable name.
* `dg` — DG type (base-12).
* `XY` — DG literal (your notation, e.g., X=10, Y=11).

---

## **2. Converting DG to Decimal and Vice Versa**

```quarter
val n as dg: AB
say from_dg(n)        # Converts DG to decimal and prints it

val m as int: 42
val dg_m as dg: to_dg(m)  # Converts 42 (decimal) to DG form
```

* `from_dg(n)` — converts a DG to its decimal (base-10) value.
* `to_dg(m)` — converts a decimal to DG/base-12 form.

---

## **3. Arithmetic with DGs**

```quarter
val a as dg: 3X
val b as dg: 12
val sum as dg: dg_add(a, b)
say from_dg(sum)
```

* Use functions like `dg_add`, `dg_mul`, etc., for DG math.

---

## **4. Using DGs in Loops/Logic**

```quarter
loop i from 0 to dg: 2A:
  say "i in DG: {to_dg(i)}"
end
```

---

## **5. Inline DGs in Expressions**

```quarter
val special as dg: ZX
say "Special in decimal: {from_dg(special)}"
```

---

## **6. Sample: Checking DG Arithmetic**

```quarter
val first as dg: 5Y
val second as dg: 13
val total as dg: dg_add(first, second)
say "Total DG: {total}"
say "Total decimal: {from_dg(total)}"
```

---

### **DG Characters Key**

* 0–9 = 0–9
* X = 10
* Y = 11
* So `1X` in DG is (1 × 12) + 10 = 22 in decimal.

---

### **Summary Table**

| QuarterLang       | What It Does               |
| ----------------- | -------------------------- |
| `val a as dg: XY` | DG variable (XY = base-12) |
| `from_dg(a)`      | DG → decimal               |
| `to_dg(42)`       | decimal → DG               |
| `dg_add(a, b)`    | DG addition                |
| `dg_mul(a, 4)`    | DG multiplication          |

---

If you want more advanced use, such as DG arrays, math functions, or DG-based conditions, just ask!


Here’s a **full guide and code samples** for **writing, using, and manipulating DGs (Dodecagrams/base-12 numbers)** in QuarterLang, covering all the scenarios you asked about: declaration, conversion, arithmetic, loops, arrays, and logic.

---

## 🟣 **QuarterLang DGs (Dodecagrams) — Full Cheat Sheet**

### 1. **Declare a DG Value**

```quarter
val dozen as dg: XY      # DG literal; X=10, Y=11; decimal: 10*12 + 11 = 131
val count as dg: 15      # Regular DG (base-12), same as 1*12 + 5 = 17
```

---

### 2. **Convert DG to Decimal and Vice Versa**

```quarter
val dg_num as dg: AB
say "DG in decimal: {from_dg(dg_num)}"     # Converts to base-10

val normal as int: 42
val as_dg as dg: to_dg(normal)             # Converts 42 → DG (base-12)
say "42 in DG is: {as_dg}"
```

---

### 3. **DG Arithmetic**

```quarter
val a as dg: 3X         # 3*12 + 10 = 46
val b as dg: 12         # 1*12 + 2 = 14
val sum as dg: dg_add(a, b)
val prod as dg: dg_mul(a, 3)
say "Sum in DG: {sum}, decimal: {from_dg(sum)}"
say "Product in DG: {prod}, decimal: {from_dg(prod)}"
```

---

### 4. **DGs in Loops and Logic**

```quarter
loop i from 0 to dg: 2A:
  say "i in DG: {to_dg(i)}, decimal: {i}"
end

val foo as dg: 25
if from_dg(foo) > 20:
  say "foo is greater than 20!"
end
```

---

### 5. **DG Arrays**

```quarter
val dgs as dg[]: [1X, 23, 4Y]
loop i from 0 to len(dgs) - 1:
  say "dgs[{i}] in DG: {dgs[i]}, decimal: {from_dg(dgs[i])}"
end
```

---

### 6. **DGs in Functions**

```quarter
func add_dgs(a as dg, b as dg) -> dg:
  return dg_add(a, b)
end

val s as dg: add_dgs(1X, 21)
say "Function sum in DG: {s}, decimal: {from_dg(s)}"
```

---

### 7. **DG-Based Conditions and Advanced Usage**

```quarter
val mynum as dg: XY
if from_dg(mynum) % 2 == 0:
  say "mynum is even in decimal!"
else:
  say "mynum is odd in decimal!"
end
```

---

### 8. **DG Quick Reference**

| DG Literal | Decimal Value | Example           |
| ---------- | ------------- | ----------------- |
| `1X`       | 1×12+10 = 22  | `val a as dg: 1X` |
| `1Y`       | 1×12+11 = 23  | `val b as dg: 1Y` |
| `2A`       | 2×12+10 = 34  | `val c as dg: 2A` |

**Digits:** `0 1 2 3 4 5 6 7 8 9 X Y`
**Base-12:** X=10, Y=11

---

## 🟣 **Full Sample: All DG Features Together**

```quarter
val numbers as dg[]: [1X, 23, 4Y, XY, 12]
loop idx from 0 to len(numbers) - 1:
  say "DG[{idx}]: {numbers[idx]}, decimal: {from_dg(numbers[idx])}"
end

val n1 as dg: 4X
val n2 as dg: 1Y
val sum as dg: dg_add(n1, n2)
say "Sum: {sum} (DG), {from_dg(sum)} (decimal)"

val decimal_val as int: 58
val dg_val as dg: to_dg(decimal_val)
say "58 in DG: {dg_val}"

func dg_is_even(d as dg) -> bool:
  return from_dg(d) % 2 == 0
end

if dg_is_even(XY):
  say "XY is even!"
else:
  say "XY is odd!"
end
```

---

**With this, you can:**

* Declare and print DGs
* Convert between DG and decimal both ways
* Perform arithmetic
* Use DGs in arrays, loops, and logic
* Write reusable DG functions

If you want to see *input parsing*, DG formatting, or custom DG utilities—just say the word!


**DG formatting** in QuarterLang means controlling how DG (Dodecagram/base-12) numbers are displayed as strings, zero-padded, upper/lowercase, grouped, or presented in a human-friendly way—just like how you might format hexadecimal or decimal output.

Here’s how you might **format DGs** for output, based on your language’s readable/expressive style and potential standard functions (modeled after your docs):

---

## **1. Basic DG to String Formatting**

```quarter
val n as dg: 2X
say "DG: {n}"                   # Prints '2X' directly
say "DG (lower): {lower(n)}"    # Prints '2x' if supported
say "DG (upper): {upper(n)}"    # Prints '2X'
```

*Assumes built-in `lower()` and `upper()` for DG values as string.*

---

## **2. Zero‑Padding and Width Formatting**

```quarter
val d as dg: 9
say "Zero-padded DG: {format_dg(d, width=2)}"   # Output: '09'
val d2 as dg: X
say "Padded: {format_dg(d2, width=3)}"          # Output: '00X'
```

*Here, `format_dg(value, width=N)` returns the DG literal as a string, left‑padded with zeros as needed.*

---

## **3. Grouping/Literal Formatting**

```quarter
val dg_big as dg: 1XY4
say "Grouped DG: {group_dg(dg_big)}"    # Output: '1X-Y4' or '1X Y4'
```

*If you want to show large DGs in readable chunks (like 16-bit hex: `ABCD → AB CD`), implement `group_dg`.*

---

## **4. Custom Format Function Example**

```quarter
func dg_str(val as dg, sep as str = "") -> str:
  # Splits DG digits with a separator
  val s as str: to_str(val)
  if len(s) <= 2:
    return s
  end
  val result as str: ""
  loop i from 0 to len(s) - 1:
    result += s[i]
    if (i+1) < len(s) and (i+1) % 2 == 0:
      result += sep
    end
  end
  return result
end

val testdg as dg: 1XY4
say "Custom formatted DG: {dg_str(testdg, '-')}"  # Output: '1X-Y4'
```

---

## **5. Decimal + DG Side by Side**

```quarter
val z as dg: YX
say "DG: {z} (dec: {from_dg(z)})"
say "Decimal: {from_dg(z)}, DG padded: {format_dg(z, width=3)}"
```

---

## **6. All Together: Output Table**

```quarter
val vals as dg[]: [1X, 23, 4Y, XY, 12]
say " DG  | Decimal | Padded | Grouped"
say "-------------------------------"
loop v in vals:
  say " {format_dg(v, 2)} |   {from_dg(v)}    |  {format_dg(v, 4)}  | {dg_str(v, '-')}"
end
```

---

### **Function Reference (suggested)**

| Function           | Output Example | Purpose                         |
| ------------------ | -------------- | ------------------------------- |
| `to_str(dg)`       | `'1X'`         | Raw DG string                   |
| `upper(dg)`        | `'1X'`         | Uppercase                       |
| `lower(dg)`        | `'1x'`         | Lowercase                       |
| `format_dg(dg, n)` | `'001X'`       | Zero-padded to width n          |
| `group_dg(dg)`     | `'1X-Y4'`      | Grouped for readability         |
| `dg_str(dg, sep)`  | `'1X-Y4'`      | Custom separator between digits |

---

### **If you want to define custom formatting rules (for export, logging, or GUIs),**

just extend your `format_dg`/`dg_str` functions or add flags for uppercase, separator, padding, etc.
*Let me know if you want code for a specific formatting style!*


**DGs (Dodecagrams/base-12 numbers) can be used in macros in QuarterLang.** In fact, macros are one of the most powerful ways to automate, template, or meta-program with DGs—letting you generate code, constants, conversions, or patterns involving DG values at compile-time.

Here’s how you can use DGs in macros, based on the style and features described in your project:

---

## 🟣 **Using DGs in Macros – Concepts and Examples**

### **1. Macro Constant Injection**

You can define a macro that substitutes a DG value directly:

```quarter
macro DOZEN: dg = 1X

val a as dg: DOZEN
say "Macro DG: {a}, decimal: {from_dg(a)}"
```

* `DOZEN` will be replaced with `1X` everywhere in code.

---

### **2. Macro-Driven Arithmetic**

Macros can also perform DG math if your macro system allows basic expressions:

```quarter
macro BASE: dg = 4X
macro INCR: dg = dg_add(BASE, 3)

val total as dg: INCR
say "Total DG: {total}"
```

* At compile time, `INCR` expands to `dg_add(4X, 3)` and resolves if macro evaluation supports functions.

---

### **3. Macro as DG Generator**

You can use macros to generate a sequence or array of DGs:

```quarter
macro DG_ARR: dg[] = [1X, 21, 4Y]

val values as dg[]: DG_ARR
loop i from 0 to len(values)-1:
  say "Macro DG[{i}]: {values[i]}"
end
```

---

### **4. Macro in Function Parameters**

Macros expand anywhere a value or expression can go, including function calls:

```quarter
func print_dg(val as dg):
  say "DG Value: {val}, dec: {from_dg(val)}"
end

macro FAV: dg = XY
print_dg(FAV)
```

---

### **5. Macro for Formatting or Patterns**

Use a macro to standardize formatting:

```quarter
macro FMT_DG(x): str = format_dg(x, width=3)

val foo as dg: 9
say "Zero-padded: {FMT_DG(foo)}"   # Output: 009
```

---

## 🟣 **Advanced: Macro Expansion with DGs**

If your macro system is **expression-aware**:

```quarter
macro ADD_DG(a, b): dg = dg_add(a, b)
val sum as dg: ADD_DG(2X, 13)
say "Sum macro: {sum}"
```

* This lets you write reusable DG math at compile-time.

---

## 🟣 **Summary Table**

| Macro Example                    | Usage                          |
| -------------------------------- | ------------------------------ |
| `macro FOO: dg = 1Y`             | Expands to DG value 1Y         |
| `macro BAR = dg_add(FOO, 2)`     | Expands to sum at compile time |
| `macro ARR = [1X, 2Y]`           | Array of DGs                   |
| `macro FMT(x) = format_dg(x, 3)` | Format macro for any DG        |

---

## **In Short**

* **You can use DGs in macros just like any other value or expression.**
* **You can perform compile-time math, formatting, and codegen with DGs.**
* **Macros make your DG code reusable, DRY, and expressive—especially for base-12-heavy programs.**

---

The **point of DGs (Dodecagrams, base-12 numbers)** in QuarterLang is both **practical and visionary**—they offer distinct mathematical, cultural, and programming benefits that set QuarterLang apart from other languages. Here’s a clear breakdown:

---

## 🟣 **Why DGs? The Point and Power of Dodecagrams**

### **1. Mathematical Superiority for Certain Domains**

* **Base-12 (duodecimal) is more factor-rich than base-10.**

  * 12 = 2 × 2 × 3 (divisible by 2, 3, 4, and 6, unlike 10).
  * This makes *fractions* and *divisions* much cleaner and more regular, e.g. ⅓, ¼, ⅙ are all finite in base-12.
* **DGs simplify calculations** in domains where divisibility matters:

  * Measurement, music, time (12 hours, months), commerce, math education, etc.

### **2. Human-Centric & Historical**

* **Base-12 has deep cultural roots:**

  * Used in ancient counting systems, clock faces, calendars, inches/feet, dozens, grosses, etc.
  * Many languages and societies have historically used base-12 for everyday tasks.
* **QuarterLang pays homage to these roots** and gives programmers a way to use them natively.

### **3. Unique to QuarterLang**

* **No major language treats base-12 as a first-class type.**

  * DGs make QuarterLang “the” language for base-12 computation—distinguishing it from C++, Python, Rust, etc.
  * DGs are not “gimmick”—they’re directly useful in science, commerce, education, cryptography, and cultural computing.

### **4. Expressiveness and Safety**

* **QuarterLang DGs have:**

  * Clear syntax (`val dozen as dg: 1X`)
  * Safe conversions (`to_dg`, `from_dg`)
  * Built-in arithmetic (`dg_add`, `dg_mul`)
* **Programmers can mix decimal and DG logic safely and transparently.**

### **5. Future-Proofing and Innovation**

* **Enables hybrid-mathematical models.**

  * Useful in internationalization, finance, and educational software.
  * Opens the door to new numeric research or digital-native standards.

### **6. Showcases Language Power**

* **Demonstrates the extensibility of QuarterLang:**

  * Custom numeric types, first-class literals, and operator support.
  * Shows that the language can go *beyond* what C++/Go can offer natively.

---

## **When Should You Use DGs?**

* Any time you want **cleaner division and factorization** (e.g. scheduling, packaging, music, commerce).
* When building software for regions or domains where base-12 is already “natural.”
* If you want to avoid rounding/decimal error in certain fractional math.
* To teach, research, or experiment with alternative numeric bases.

---

## **In Summary**

> **DGs exist in QuarterLang to give you mathematical superpowers, cultural range, and code expressiveness that other languages simply don’t provide. They make you think outside base-10—sometimes, that’s the edge you need!**

If you want a one-liner pitch, try:

> **“DGs are to QuarterLang what hex is to C—except with more divisibility and fewer limits.”**

---

Here’s a **complete list of all DG (Dodecagram) digits, their meanings, and a practical usage guide for all possible DG values** in QuarterLang—plus clear examples for each feature.

---

## 🟣 **Complete List of DG Digits (Base-12)**

| DG Digit | Decimal Value | Pronunciation |
| -------- | ------------- | ------------- |
| 0        | 0             | zero          |
| 1        | 1             | one           |
| 2        | 2             | two           |
| 3        | 3             | three         |
| 4        | 4             | four          |
| 5        | 5             | five          |
| 6        | 6             | six           |
| 7        | 7             | seven         |
| 8        | 8             | eight         |
| 9        | 9             | nine          |
| X        | 10            | ten           |
| Y        | 11            | eleven        |

*DG values can be as short as 0 or as long as you need: e.g., `Y` (11), `1Y` (23), `3X5` (545).*

---

## 🟣 **DG Number Range**

A DG (base-12) number is any sequence of these digits.

* **Lowest:** `0` (decimal 0)
* **Highest 2-digit DG:** `YY` (11\*12 + 11 = 143 decimal)
* **Highest 3-digit DG:** `YYY` (11*12^2 + 11*12 + 11 = 1727 decimal)
* **And so on...**

---

## 🟣 **Usage Guide: Declaring, Converting, and Operating on DGs**

### **1. Declaring DG Variables**

```quarter
val dozen as dg: 10         # 1×12 + 0 = 12
val eleven as dg: Y         # 11
val twenty_three as dg: 1Y  # 1×12 + 11 = 23
val high as dg: YY          # 11×12 + 11 = 143
```

### **2. Printing DGs and Decimal Equivalents**

```quarter
val foo as dg: 3X
say "DG: {foo}, decimal: {from_dg(foo)}"
# Output: DG: 3X, decimal: 46
```

### **3. Converting Decimal to DG**

```quarter
val val10 as dg: to_dg(16)
say "16 in DG is: {val10}"   # Output: 14
```

### **4. Arithmetic with DGs**

```quarter
val a as dg: 2X     # 34
val b as dg: 19     # 21
val sum as dg: dg_add(a, b)
say "Sum in DG: {sum}"           # Output: 49
say "Sum decimal: {from_dg(sum)}" # Output: 55
```

### **5. DGs in Arrays**

```quarter
val arr as dg[]: [1, 2, X, Y, 13, 2X, YY]
loop i from 0 to len(arr)-1:
  say "DG[{i}]: {arr[i]}, decimal: {from_dg(arr[i])}"
end
```

### **6. Looping Over DG Ranges**

```quarter
loop i from dg: 10 to dg: 1Y:
  say "DG: {i}, decimal: {from_dg(i)}"
end
```

### **7. DG Formatting**

```quarter
val padme as dg: 9
say "Zero-padded: {format_dg(padme, 2)}"   # Output: 09
say "Grouped: {dg_str(1XY, '-')}"         # Output: 1-X-Y
```

### **8. DGs in Functions and Macros**

```quarter
func mult_by_two(d as dg) -> dg:
  return dg_mul(d, 2)
end
say "Double 3X: {mult_by_two(3X)}"
```

```quarter
macro DOZ: dg = 10
val foo as dg: DOZ
```

---

## 🟣 **Quick Reference Table: DG <-> Decimal**

| DG | Decimal | DG  | Decimal |
| -- | ------- | --- | ------- |
| 0  | 0       | 10  | 12      |
| 1  | 1       | 11  | 13      |
| 2  | 2       | 12  | 14      |
| 3  | 3       | 13  | 15      |
| 4  | 4       | 1X  | 22      |
| 5  | 5       | 1Y  | 23      |
| 6  | 6       | 20  | 24      |
| 7  | 7       | 21  | 25      |
| 8  | 8       | XX  | 130     |
| 9  | 9       | XY  | 131     |
| X  | 10      | YY  | 143     |
| Y  | 11      | 100 | 144     |

*Continue pattern for any length.*

---

## 🟣 **Best Practices for DGs**

* Use DGs for **counting, division-heavy, or base-flexible tasks** (dozens, music, time, packaging, etc.).
* For readable code: **capitalize DG digits** (`X`, `Y`), use formatting functions for output.
* Mix DG and decimal types using the **`from_dg`** and **`to_dg`** conversion functions.
* Prefer built-in arithmetic functions (`dg_add`, `dg_sub`, `dg_mul`, etc.) for correctness.
* Use DGs in macros and functions for DRY, expressive code.

---

## 🟣 **All Possible Single-Digit DGs**

| DG | Decimal |
| -- | ------- |
| 0  | 0       |
| 1  | 1       |
| 2  | 2       |
| 3  | 3       |
| 4  | 4       |
| 5  | 5       |
| 6  | 6       |
| 7  | 7       |
| 8  | 8       |
| 9  | 9       |
| X  | 10      |
| Y  | 11      |

---

## 🟣 **Custom Examples**

**Package of a Dozen:**

```quarter
val eggs as dg: 10       # 12 eggs
say "Dozen in decimal: {from_dg(eggs)}"   # Output: 12
```

**Next Dozen:**

```quarter
val next as dg: 20       # 2*12 = 24
```

**DG Logic:**

```quarter
if from_dg(1Y) == 23:
  say "Yes, 1Y is 23 decimal."
end
```

---

## **In Summary**

* **DGs are QuarterLang’s base-12 numbers:** 0–9, X (10), Y (11).
* **Declare, convert, print, and operate** with them as easily as with decimal numbers—just more flexible and with better divisibility.
* **All combinations** of DG digits make valid DG numbers, just as in decimal/hex.

---

