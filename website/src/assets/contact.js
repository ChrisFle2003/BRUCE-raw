// No message logging, browser storage, automatic retries, or private credentials.
const form = document.querySelector('#contact-form');
if (form && form.dataset.submissionMode === 'ajax' && window.fetch && window.AbortController) {
  const fields = ['name', 'email', 'subject', 'message'].map(name => form.elements.namedItem(name));
  const button = form.querySelector('[type="submit"]');
  const status = form.querySelector('#contact-status');
  let pending = false;
  let accepted = false;
  form.noValidate = true; // The native required/type attributes remain for the HTML fallback.
  const announce = (message, state, focus = false) => {
    status.textContent = message;
    status.dataset.state = state;
    if (focus) status.focus();
  };
  const clearErrors = () => fields.forEach(field => {
    field.removeAttribute('aria-invalid');
    document.getElementById(`${field.id}-error`).textContent = '';
  });
  const mark = (field, message) => {
    field.setAttribute('aria-invalid', 'true');
    document.getElementById(`${field.id}-error`).textContent = message;
  };
  const messages = {
    name: 'Enter your name.',
    email: 'Enter a valid email address, such as name@example.com.',
    subject: 'Keep the subject within 200 characters.',
    message: 'Enter your message.'
  };
  const uncertain = 'We could not confirm your submission. Your entries are still here. Please try again later; retrying may send a duplicate.';
  for (const field of fields) field.addEventListener('input', () => {
    field.removeAttribute('aria-invalid');
    document.getElementById(`${field.id}-error`).textContent = '';
    if (accepted) { accepted = false; button.disabled = false; announce('Your edits have not been sent.', 'idle'); }
  });
  form.addEventListener('submit', async event => {
    event.preventDefault();
    if (pending || accepted) return;
    clearErrors();
    const invalid = [];
    for (const field of fields) {
      if ((field.required && !field.value.trim()) || !field.validity.valid || field.value.length > field.maxLength) {
        invalid.push(field);
        mark(field, field.value.length > field.maxLength ? `Use ${field.maxLength.toLocaleString('en')} characters or fewer.` : messages[field.name]);
      }
    }
    if (invalid.length) {
      announce('Check the highlighted fields, then try again.', 'error');
      invalid[0].focus(); return;
    }
    // Capture values before locking editing; pending guards Enter and programmatic re-submits.
    const data = new FormData(form);
    pending = true;
    button.disabled = true;
    fields.forEach(field => { field.readOnly = true; });
    announce('Sending your message…', 'pending');
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), 25000);
    try {
      const response = await fetch(form.action, {
        method: 'POST', body: data, headers: { Accept: 'application/json' },
        credentials: 'omit', redirect: 'error', signal: controller.signal
      });
      if (response.status === 429) {
        announce('Too many submissions right now. Please wait before trying again. Your entries are still here.', 'error', true);
        return;
      }
      const payload = await response.json();
      const errors = Array.isArray(payload?.errors) ? payload.errors : [];
      if (response.ok && payload?.ok === true && errors.length === 0 && !payload.error) {
        accepted = true;
        announce('Thank you. Formspree accepted your message.', 'success', true);
        return;
      }
      const rejected = response.status === 400 || response.status === 422;
      const invalidFields = [];
      if (rejected) for (const error of errors) {
        const field = fields.find(field => field.name === error.field);
        if (field) {
          // Never inject provider-supplied text/HTML into the page.
          mark(field, ['REQUIRED_FIELD_EMPTY', 'REQUIRED_FIELD_MISSING', 'TYPE_EMAIL'].includes(error.code) ? messages[field.name] : 'Please check this field. Formspree did not accept its value.');
          invalidFields.push(field);
        }
      }
      if (invalidFields.length) {
        announce('Check the highlighted fields. Formspree did not accept the submission; your entries are still here.', 'error');
        // Focus after finally restores editing.
        setTimeout(() => invalidFields[0].focus(), 0);
      } else {
        announce(uncertain, 'error', true);
      }
    } catch {
      // A missing acknowledgement is not evidence that the service received nothing.
      announce(uncertain, 'error', true);
    } finally {
      clearTimeout(timer);
      pending = false;
      fields.forEach(field => { field.readOnly = false; });
      button.disabled = accepted;
    }
  });
}
