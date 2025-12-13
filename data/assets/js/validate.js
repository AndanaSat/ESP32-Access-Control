const validateForm = (form) => {
    form.setAttribute('novalidate', '');

    const rows = form.querySelectorAll('.row');

    const validateRow = (row) => {
        const input = row.querySelector('input');
        const errorMsg = row.querySelector('.note, #error-message');
        let rowValid = true;

        if (!input) {
            return true;
        }

        if (errorMsg) {
            errorMsg.textContent = '';
        }

        if (input.hasAttribute('required') && input.value.trim() === '') {
            if (errorMsg) {
                errorMsg.textContent = 'This field is required.';
                rowValid = false;
            }
        }
        if (rowValid && input.hasAttribute('minlength')) {
            const min = parseInt(input.getAttribute('minlength'))
            if (input.value.length < min) {
                errorMsg.textContent = errorMsg.textContent = `Minimum length is ${min} characters.`;
                rowValid = false;
            }
        }

        return rowValid;
    }

    let allValid = true;

    rows.forEach(row => {
        if (!validateRow(row)) {
            allValid = false;
        }

        const input = row.querySelector("input");

        if (!input) {
            return;
        }

        input.addEventListener('input', () => {
            validateRow(row);
        });
    });

    return allValid;
}

export { validateForm }