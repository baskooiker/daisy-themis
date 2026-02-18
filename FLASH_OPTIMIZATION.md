# Firmware FLASH Optimization Notes

Current: 127,776 B / 128 KB = 97.49% (after Font7x10 removal + std::function cleanup)

## Completed

### Remove Font7x10 (~1,900 bytes saved)
Font7x10 was only used for the startup splash screen text. Switched to Font6x8 which is
already used for all menu rendering. The linker garbage-collected the entire 1,900-byte font table.

### Replace std::function with function pointer in TR-8 (~900 bytes saved)
`ProcessTR8Step()` took a `std::function<void(uint8_t, uint8_t)>` callback. Changed to a
plain `TR8TriggerCallback` typedef (function pointer). This eliminated `std::_Function_handler`,
`_M_invoke`, `_M_manager`, and `__throw_bad_function_call` overhead. Both firmware and desktop
call sites use non-capturing lambdas that decay to function pointers.

## Future Opportunities

### 1. Shrink UpdateDisplay() — estimated 2-3 KB savings
`UpdateDisplay()` is the largest application function at 9,384 bytes. It renders all menu
states with repeated `snprintf` + `SetCursor` + `WriteString` sequences.

**Approach**: Extract a small helper like:
```cpp
void DrawLine(uint8_t y, const char* text, bool selected = false) {
    hw.display.SetCursor(0, y);
    if (selected) { /* invert line */ }
    hw.display.WriteString(text, Font_6x8, true);
}
```
Many menu states render scrollable lists with the same pattern. A shared `RenderMenuList()`
that takes an array of names, count, selected index, and scroll offset could eliminate
hundreds of lines of duplicated code.

### 2. Reduce chord progressions data — up to 1,880 bytes
`progressions` is 1,880 bytes of chord progression data. Options:
- Remove least-used progressions
- Compress: many progressions share common sub-patterns
- Generate some programmatically instead of storing as const data

### 3. Trim groove/velocity patterns — up to 1,024 bytes
`groovePatterns[32][16]` (512B) + `velocityPatterns[32][16]` (512B). Many of these 32
patterns are similar. Reducing to 16 patterns would save 512 bytes with minimal musical
impact.

### 4. Reduce bass pitch patterns — up to 640 bytes
`bassPitchPatterns` is 640 bytes. Could reduce the number of patterns or compress shared
sequences.

### 5. Other std::function callbacks in sequencer
The desktop sequencer (`themis_sequencer.h`) still uses `std::function` for other callbacks
(`onDrumTrigger`, `onMelodyTrigger`, `onChordTrigger`, `onBassTrigger`, `onRhythmTrigger`).
These don't affect firmware FLASH (firmware doesn't use the core sequencer), but converting
them to function pointers would keep the codebase consistent.

### 6. Unused display methods linked via template instantiation (~2,050 bytes, hard)
`DrawArc` (1,074B) and `DrawRect` (976B) are never called from Themis code but are linked
because they're weak symbols from the Daisy OLED display template class instantiation.
Eliminating these would require changes to the Daisy library itself (e.g., splitting the
display class into smaller units).

## Symbol Size Reference (largest consumers)

| Symbol | Size | Category |
|--------|------|----------|
| UpdateDisplay | 9,384 B | App code |
| AudioHandle::InternalCallback | 3,440 B | Daisy lib |
| ProcessDrumPatterns | 3,448 B | App code |
| HAL_RCCEx_PeriphCLKConfig | 3,268 B | HAL driver |
| UART_SetConfig | 2,092 B | HAL driver |
| ProcessControls | 1,944 B | App code |
| progressions | 1,880 B | Const data |
| HAL_HCD_IRQHandler | 1,892 B | HAL driver |
| HAL_PCD_IRQHandler | 1,732 B | HAL driver |
| Font6x8 | 1,520 B | Const data |
| DrawArc (unused) | 1,074 B | Daisy lib |
| DrawRect (unused) | 976 B | Daisy lib |
| ProcessChordStep | 988 B | App code |
| tr8Kits | 768 B | Const data |
| bassPitchPatterns | 640 B | Const data |
| groovePatterns | 512 B | Const data |
| velocityPatterns | 512 B | Const data |
| bassPatterns | 440 B | Const data |
