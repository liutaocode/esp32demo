[简体中文](README.zh_CN.md) | English

# Call interface icons

`call.svg` and `person.svg` are Material Design icons by Google, from the
[Material Design icons repository](https://github.com/google/material-design-icons).
They are distributed under Apache License 2.0; retain the accompanying `LICENSE`.
Sources: `src/communication/call/materialicons/24px.svg` and
`src/social/person/materialicons/24px.svg`.

`tools/generate_excuse_call_icons.js` uses Node.js and sharp to recolor and
supersample these vectors into small antialiased PNGs and `excuse_call_icons.c`.
The C descriptors store BGRA pixels for LVGL's ARGB8888 format. The handset is
white at 32 pixels; the contact icon is muted blue at 56 pixels.
