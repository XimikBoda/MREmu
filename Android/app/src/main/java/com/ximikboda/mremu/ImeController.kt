package com.ximikboda.mremu

import android.app.Activity
import android.content.Context
import android.text.Editable
import android.text.InputFilter
import android.text.InputType
import android.text.TextWatcher
import android.view.ViewGroup
import android.view.inputmethod.BaseInputConnection
import android.view.inputmethod.EditorInfo
import android.view.inputmethod.InputMethodManager
import android.widget.EditText

class ImeController(
    private val activity: Activity,
    private val onImeClosed: () -> Unit
) {
    private external fun nativeOnImeTextChanged(
        text: String,
        selStart: Int,
        selEnd: Int,
        compStart: Int,
        compEnd: Int
    )

    private var isSyncingFromNative = false

    private val hiddenEditText = object : EditText(activity) {
        override fun onSelectionChanged(selStart: Int, selEnd: Int) {
            super.onSelectionChanged(selStart, selEnd)
            if (!isSyncingFromNative && text != null) {
                val editable = text
                val compStart = BaseInputConnection.getComposingSpanStart(editable)
                val compEnd = BaseInputConnection.getComposingSpanEnd(editable)
                nativeOnImeTextChanged(editable.toString(), selStart, selEnd, compStart, compEnd)
            }
        }
    }.apply {
        isFocusable = true
        isFocusableInTouchMode = true
        alpha = 0f
        translationX = -10000f
        imeOptions = EditorInfo.IME_FLAG_NO_EXTRACT_UI or EditorInfo.IME_FLAG_NO_FULLSCREEN

        addTextChangedListener(object : TextWatcher {
            override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) {}
            override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) {}
            override fun afterTextChanged(s: Editable?) {
                if (isSyncingFromNative || s == null) return
                val compStart = BaseInputConnection.getComposingSpanStart(s)
                val compEnd = BaseInputConnection.getComposingSpanEnd(s)
                nativeOnImeTextChanged(
                    s.toString(),
                    selectionStart,
                    selectionEnd,
                    compStart,
                    compEnd
                )
            }
        })
    }

    init {
        activity.addContentView(hiddenEditText, ViewGroup.LayoutParams(1, 1))
    }

    fun open(initialText: String, maxSize: Int, inputTypeFlags: Int) {
        activity.runOnUiThread {
            isSyncingFromNative = true

            hiddenEditText.filters = if (maxSize > 0) {
                arrayOf(InputFilter.LengthFilter(maxSize))
            } else {
                emptyArray()
            }

            hiddenEditText.inputType = if (inputTypeFlags != 0) {
                inputTypeFlags
            } else {
                InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_FLAG_MULTI_LINE
            }

            hiddenEditText.setText(initialText)
            hiddenEditText.setSelection(initialText.length)
            isSyncingFromNative = false

            hiddenEditText.requestFocus()
            val imm = activity.getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
            imm.showSoftInput(hiddenEditText, InputMethodManager.SHOW_IMPLICIT)
        }
    }

    fun close() {
        activity.runOnUiThread {
            val imm = activity.getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
            imm.hideSoftInputFromWindow(hiddenEditText.windowToken, 0)
            hiddenEditText.clearFocus()
            onImeClosed()
        }
    }

    fun setSelection(pos: Int) {
        activity.runOnUiThread {
            isSyncingFromNative = true
            val clamped = pos.coerceIn(0, hiddenEditText.text.length)
            hiddenEditText.setSelection(clamped)
            isSyncingFromNative = false
        }
    }

    fun setText(newText: String, cursorPos: Int) {
        activity.runOnUiThread {
            isSyncingFromNative = true
            hiddenEditText.setText(newText)
            val clamped = cursorPos.coerceIn(0, newText.length)
            hiddenEditText.setSelection(clamped)
            isSyncingFromNative = false
        }
    }
}