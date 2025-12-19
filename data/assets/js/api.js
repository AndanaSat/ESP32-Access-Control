import { changeButtonToLoading } from "../js/changeButton.js"

const sendRequest = async (form, button) => {
    const formData = new FormData(form);

    changeButtonToLoading(button, true);

    try {
        const response = await fetch(form.action, {
            method: form.method,
            body: formData
        });

        const text = await response.text();
        changeButtonToLoading(button, false);

        if (!response.ok) {
            alert(`Error ${response.status}: ${text}`);
            return;
        }

        alert(text);
        window.location.reload();

    } catch (error) {
        alert(`Network Error: ${error}`);
        changeButtonToLoading(button, false);
    }
}

const rebootDevice = async (button) => {
    changeButtonToLoading(button, true);

    try {
        const response = await fetch("/reboot", { method: "POST" });
        const text = await response.text();

        changeButtonToLoading(button, false);

        if (!response.ok) {
            alert(`Error ${response.status}: ${text}`);
            return;
        }

        alert(text);
        window.location.reload();
    } catch (error) {
        alert(`Network Error: ${error}`);
        changeButtonToLoading(button, false);
    }
}

const resetDevice = async (button) => {
    changeButtonToLoading(resetButton, true);

    try {
        const response = await fetch("/reset", { method: "POST" });
        const text = await response.text();

        changeButtonToLoading(button, false);

        if (!response.ok) {
            alert(`Error ${response.status}: ${text}`);
            return;
        }

        alert(text);
        window.location.reload();
    } catch (error) {
        alert(`Network Error: ${error}`);
        changeButtonToLoading(button, false);
    }
}

export { sendRequest, rebootDevice, resetDevice }