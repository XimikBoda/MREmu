package com.ximikboda.mremu

import androidx.compose.runtime.Composable
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.snapshots.SnapshotStateList
import androidx.compose.runtime.saveable.listSaver

/**
 * Minimal saveable stack used by the options bottom sheet to page between the main
 * list and its sub pages.
 *
 * Material guidance slides *forward* when a page is pushed and *back* when it is
 * popped, so the direction is derived from the top of the stack rather than being
 * tracked separately.
 */
class PageBackStack<T>(initial: List<T>) {

    val items: SnapshotStateList<T> = mutableStateListOf<T>().apply { addAll(initial) }

    val current: T get() = items.last()

    val canGoBack: Boolean get() = items.size > 1

    fun push(value: T) {
        if (items.lastOrNull() == value) return
        items.add(value)
    }

    fun pop() {
        if (canGoBack) items.removeAt(items.lastIndex)
    }

    companion object {
        fun <T> saver() = listSaver<PageBackStack<T>, T>(
            save = { it.items.toList() },
            restore = { PageBackStack(it) }
        )
    }
}

/** Remembers a [PageBackStack] that survives configuration changes. */
@Composable
fun <T> rememberPageBackStack(vararg initial: T): PageBackStack<T> =
    rememberSaveable(saver = PageBackStack.saver<T>()) { PageBackStack(initial.toList()) }