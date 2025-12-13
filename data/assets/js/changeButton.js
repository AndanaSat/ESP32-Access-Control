const changeButtonToLoading = (button, status) => {
    if (status) {
        button.disabled   = true;
        button.classList.add("button-loading");
        return;
    } else {
        button.disabled = false;
        button.classList.remove("button-loading");
        return;
    }
}

export { changeButtonToLoading }