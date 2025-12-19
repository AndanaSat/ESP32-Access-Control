const validateForm = (form) => {
    form.setAttribute('novalidate', '');
    const rows = form.querySelectorAll('.row');

    const validateRow = (row) => {
        const input = row.querySelector('input');
        const errorMsg = row.querySelector('.note, #error-message');
        errorMsg.textContent = '';

        if (!input) {
            return true;
        }

        if (input.hasAttribute('required') && input.value.trim() === '') {
            if (errorMsg) {
                errorMsg.textContent = 'This field is required.';
                return false;
            }
        }
        if (input.hasAttribute('minlength')) {
            const min = parseInt(input.getAttribute('minlength'))
            if (input.value.length < min) {
                errorMsg.textContent = errorMsg.textContent = `Minimum length is ${min} characters.`;
                return false;
            }
        }

        return true;
    }

    let allValid = true;

    rows.forEach(row => {
        // initial validation
        if (!validateRow(row)) {
            allValid = false;
        }

        // re-validate after input change
        const input = row.querySelector("input");

        input.addEventListener('input', () => {
            validateRow(row);
        });
    })

    return allValid;
}

export { validateForm }