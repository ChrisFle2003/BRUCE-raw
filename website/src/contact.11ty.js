import { layout } from '../lib/layout.mjs';
import { getConfig } from '../lib/catalog.mjs';
import { getContactConfig } from '../lib/contact.mjs';
import { escape } from '../lib/content.mjs';

export function renderContact(contact = getContactConfig()) {
  const { base } = getConfig();
  const { available, endpoint, privacyNoticeUrl, submissionMode } = contact;
  const open = available ? `<form id="contact-form" class="contact-form" action="${escape(endpoint)}" method="post" target="_blank" rel="noopener" data-submission-mode="${submissionMode}" aria-describedby="contact-privacy">` : '<div class="contact-form">';
  return layout({ title: 'Contact', description: 'Get in touch about BRUCE_raw. Ask a question about the core, documentation, or a potential collaboration.', url: 'contact/', body: `
    <main id="main" class="contact-page wrap">
      <div class="eyebrow">A conversation starts with a question</div>
      <div class="contact-layout">
        <div class="contact-intro"><h1>Let’s talk<br><em>about the core.</em></h1><p class="doc-lead">Have a question about BRUCE, its documentation, or a possible collaboration? Leave a little context.</p>
          <div class="contact-aside"><span class="eyebrow">Before you write</span><p>The documentation may have your answer.</p><a class="text-link" href="${base}docs/">Explore the documentation →</a></div>
          <div class="contact-aside"><span class="eyebrow">Security matters</span><p>For vulnerabilities or sensitive security details, follow the project’s private reporting process.</p><a class="text-link" href="${base}docs/security/">Read the security policy →</a></div>
        </div>
        <section class="contact-panel" aria-labelledby="contact-title"><div class="contact-panel-heading"><span class="eyebrow">Contact / BRUCE_raw</span><h2 id="contact-title">Send a message</h2><p>Name, email, and message are required.</p></div>
          ${!available ? '<div class="contact-notice" role="status"><strong>Contact is not available yet.</strong><p>This form is not connected, so no message can be sent. Please check back later.</p></div>' : ''}
          ${open}
            <fieldset${available ? '' : ' disabled'}><legend class="visually-hidden">Your contact details and message</legend>
              <div class="contact-fields">
                <div class="contact-field"><label for="contact-name">Name <span>(required)</span></label><input id="contact-name" name="name" type="text" autocomplete="name" maxlength="100" required aria-describedby="contact-name-error"><p class="field-error" id="contact-name-error"></p></div>
                <div class="contact-field"><label for="contact-email">Email address <span>(required)</span></label><input id="contact-email" name="email" type="email" autocomplete="email" inputmode="email" autocapitalize="none" spellcheck="false" maxlength="254" required aria-describedby="contact-email-hint contact-email-error"><p class="field-hint" id="contact-email-hint">An address where you can receive a reply.</p><p class="field-error" id="contact-email-error"></p></div>
              </div>
              <div class="contact-field"><label for="contact-subject">Subject <span>(optional)</span></label><input id="contact-subject" name="subject" type="text" autocomplete="off" maxlength="200" aria-describedby="contact-subject-error"><p class="field-error" id="contact-subject-error"></p></div>
              <div class="contact-field"><label for="contact-message">Message <span>(required)</span></label><textarea id="contact-message" name="message" rows="7" autocomplete="off" maxlength="5000" required aria-describedby="contact-message-hint contact-message-error"></textarea><p class="field-hint" id="contact-message-hint">Up to 5,000 characters. Please leave out passwords and sensitive information.</p><p class="field-error" id="contact-message-error"></p></div>
              <div id="contact-status" class="contact-status" role="status" aria-live="polite" aria-atomic="true" tabindex="-1"></div>
              <button class="button primary contact-submit" type="${available ? 'submit' : 'button'}"${available ? '' : ' disabled'}>Send message <span aria-hidden="true">↗</span></button>
            </fieldset>
          ${available ? '</form>' : '</div>'}
          <div id="contact-privacy" class="contact-privacy"><p>${available ? 'Your name, email address, subject, and message are sent to Formspree for processing when you submit.' : 'When enabled, this form will use Formspree to process submissions.'} <a href="https://formspree.io/legal/privacy-policy/">Formspree privacy policy ↗</a></p>${privacyNoticeUrl ? `<p><a href="${escape(privacyNoticeUrl)}">Read the site’s privacy notice ↗</a></p>` : '<p class="privacy-gap">A site-specific contact privacy notice has not been published yet.</p>'}</div>
          ${available ? submissionMode === 'hosted' ? '<p class="field-hint">Submission and any security checks open on Formspree in a new tab. Your entries stay on this page; check the new tab for the result.</p>' : '<noscript><p class="field-hint">With JavaScript off, submission opens on Formspree in a new tab. Your entries stay here; check the new tab for the result.</p></noscript>' : ''}
        </section>
      </div>
    </main>${available && submissionMode === 'ajax' ? `<script src="${base}assets/contact.js" defer></script>` : ''}` });
}
export default class {
  data() { return { permalink: 'contact/index.html' }; }
  render() { return renderContact(); }
}
