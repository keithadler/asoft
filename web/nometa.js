// js-dos maps browser key code 91 to the "[" key. That is the code of the
// Command key on a Mac and the Windows key on a PC, so every Cmd+Tab,
// Cmd+Shift+4 and Win+anything typed a "[" into DOS. js-dos listens on
// the window in the bubbling phase; a capturing listener on the window
// runs first and can swallow the modifier before it gets there. Key
// combinations with Cmd held (copy, tab switching, screenshots) still
// reach the browser, which handles them before the page anyway.
(function () {
    function meta(e) {
        return e.key === "Meta" || e.key === "OS" ||
               e.keyCode === 91 || e.keyCode === 93 || e.keyCode === 224;
    }
    function swallow(e) {
        if (meta(e)) e.stopImmediatePropagation();
    }
    window.addEventListener("keydown", swallow, true);
    window.addEventListener("keyup", swallow, true);
})();
