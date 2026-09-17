# Hazel

Hazel is a lightweight, embeddable graphical notebook application built in C++ using FLTK. It provides a highly responsive, unified text editor interface with native support for live code evaluation, seamlessly bridging the gap between a standard text editor and a Jupyter-style interactive notebook.

Hazel is designed to be easily embedded as a graphical UI layer for your own custom compilers, interpreters, or data engines.

## Features

- **Unified Reactive Interface:** Say goodbye to clunky "Command" vs "Edit" modes. Hazel operates entirely as a fluid, unified text document. Just click anywhere and type.
- **Fluid Block Types:** The editor dynamically tracks your cursor's context. Instantly switch a block to Markdown with `Ctrl+M` or Code with `Ctrl+Y`.
- **Live Evaluation:** Hit `Ctrl+Enter` to evaluate the current code block, or `Ctrl+R` to evaluate the entire document from your current block downwards.
- **Protected Outputs:** Evaluated outputs and error messages are rendered natively inside the document, but are safely read-only to prevent accidental corruption.
- **Headless Testing:** Exposes a clean C API, making it easy to test document evaluation and styling logic headlessly.

## Usage and Keybindings

Hazel relies entirely on a streamlined set of keyboard shortcuts:

| Shortcut | Action |
|----------|--------|
| `Ctrl+Enter` | Evaluate the code block your cursor is currently resting inside. |
| `Ctrl+R` | Evaluate the document from your current block downwards. |
| `Ctrl+M` | Convert the current block to a Markdown text cell. |
| `Ctrl+Y` | Convert the current block to an executable Code cell. |
| `Ctrl+Q` | Quit the application (prompts if there are unsaved changes). |
| `Up/Down/Left/Right` | Standard unified text navigation. |

## Architecture

Hazel's core architecture centers around a custom extension of `Fl_Text_Editor` (`HazelEditor`) and a parallel style buffer. 

- **State Machine:** Instead of isolating cells into individual widgets (like Jupyter), Hazel stores the entire notebook as a single contiguous `Fl_Text_Buffer`.
- **Parallel Styling:** It uses a parallel style buffer where every byte of text is mapped to a character representing its semantic block type (`A` for Input, `B` for Output, `C` for Error, `D` for Markdown).
- **Just-in-Time Rendering:** Empty lines dynamically infer their background color based on the semantic boundaries of the block the cursor rests in, ensuring a flicker-free UI experience.
- **Extensible C-API:** Code evaluation is handled by passing a callback (`hazel_eval_cb_t`) to the engine. When the user hits `Ctrl+Enter`, Hazel extracts the code block, handles the layout math, and hands the raw string off to your callback. Your engine computes the result and calls `appendOutput` to push the result back into the UI.

## Embedding Example

```cpp
#include "hazel/api.h"

// Your custom evaluation logic
void my_eval_engine(const char* input, hazel_ctx_t* ctx, void* user_data) {
    // Process the code...
    const char* result = "Evaluated output!";
    
    // Push it back to the notebook UI
    hazel_append_output(ctx, result, 0); 
    hazel_finish_eval(ctx);
}

int main() {
    // Spin up the Hazel window
    hazel_app_t* app = hazel_create("My Custom Notebook", my_eval_engine, nullptr);
    hazel_run(app);
    return 0;
}
```

## Building

Hazel requires `cmake` and `FLTK 1.4`. 

```bash
mkdir build
cd build
cmake ..
make
```

## License

MIT License. See `LICENSE` for details.
