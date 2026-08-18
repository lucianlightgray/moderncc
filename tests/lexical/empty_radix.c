/* T-mac-30213: a radix prefix (0x / 0b / 0o) with ZERO following digits is
 * invalid — clang/gcc both "error: invalid suffix". mcc entered the digit loop
 * with no >=1-digit check, so a non-digit broke it immediately and the literal
 * silently became 0. This file must be REJECTED. */
int h = 0x;
