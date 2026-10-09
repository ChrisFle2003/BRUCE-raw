# Contact form integration

**IMPLEMENTED AND VERIFIED:** the English contact page, static form markup,
client validation and submission states are covered by local automated tests
with intercepted responses. All tests block real Formspree requests.

**IMPLEMENTED, BUT NOT CURRENTLY VERIFIED:** acceptance by a live Formspree form,
its spam rules, recipient notification, and email delivery. No real message has
been sent. The checked-in form is deliberately unavailable until configured.

## One configuration location

Edit [contact.config.json](contact.config.json). Do not put credentials there.

| Setting | Purpose |
| --- | --- |
| `endpoint` | Paste the public form endpoint copied from your Formspree dashboard. Empty by default. Only HTTPS `formspree.io/f/` form-ID URLs are accepted. |
| `serverChecksConfigured` | Set to `true` only after completing the provider-side validation and spam configuration below. This is an operator declaration, not automatic verification of the remote settings. |
| `privacyNoticeUrl` | The complete HTTPS URL of your published site privacy notice. Empty by default; the page visibly flags its absence. |
| `submissionMode` | `ajax` enhances submission in place. `hosted` uses ordinary HTML submission, including Formspree-hosted security checks, in a new tab. |

The endpoint is public and appears in the HTML `action`. No recipient address,
private API key, login token, or mail-service credential belongs in this site.
An empty endpoint or unconfirmed server setup renders disabled fields with no
form action; JavaScript cannot accidentally submit to the current page.

## Provider setup

1. Create a Formspree account, project, and form. Add and verify the intended
   recipient under Linked Emails, then configure an enabled Email action in the
   form's Workflow. No recipient has been chosen in this repository.
   See [recipient setup](https://help.formspree.io/articles/form-and-project-settings/changing-a-form-email-address/).
2. Add server-side Workflow validation rules using these exact field names:

   | Field | Type | Required | Maximum length |
   | --- | --- | --- | --- |
   | `name` | Text | Yes | 100 |
   | `email` | Email | Yes | 254 |
   | `subject` | Text | No | 200 |
   | `message` | Text | Yes | 5000 |

   Match the website's field limits. Required/empty and email checks must run at
   Formspree, not just in the browser. See
   [Workflow validation](https://help.formspree.io/articles/building-your-form/getting-started-with-workflow/).
3. Keep Formspree's server-side Formshield spam filtering active. The site does
   not treat a hidden field or browser validation as spam protection. Advanced
   Formshield tuning depends on the account plan; see
   [spam filtering](https://help.formspree.io/articles/advanced-features/advanced-spam-filtering).
   For forms requiring a CAPTCHA, select `hosted` mode and retain the provider's
   CAPTCHA protection. This site does not embed Google scripts or provide a
   client CAPTCHA token. Do not switch off required protection just to make an
   AJAX request pass. See
   [Formspree-hosted CAPTCHA](https://help.formspree.io/articles/form-and-project-settings/recaptcha-settings).
4. In the project's **Restrict to Domain** setting, use
   `chrisfle2003.github.io`, without a protocol or `/My-Bruce-AI/` path. For a
   custom domain, use that domain instead. Formspree documents that disallowed
   origins go to the spam inbox; this is not authentication or a guarantee of
   delivery. Do not weaken production restrictions for local tests. See
   [domain restrictions](https://help.formspree.io/articles/form-and-project-settings/restrict-to-domain).
5. Supply an accurate site contact/privacy notice and put its URL in the config.
   The provider notice linked on the page identifies third-party processing but
   does not replace the site's own notice. No retention period, legal basis,
   reply deadline, or delivery guarantee has been invented. Formspree may also
   process request metadata; review its
   [privacy policy](https://formspree.io/legal/privacy-policy/).
6. Copy the real public endpoint into the config and mark server setup complete.
   Rebuild and run the checks. Publishing remains a separate authorized action.
   Send a live test only when explicitly authorized, and verify both provider
   acceptance and recipient receipt separately before claiming delivery.

No Formspree account or remote settings were created or changed by this work.

## Submission behavior

The normal HTML form uses POST. Without JavaScript (or in `hosted` mode), the
provider opens in a new tab, leaving the original form and its values intact.
The provider page handles validation, security checks, and the result. This
site does not infer success from opening that tab. Hosted mode retains native
browser required/email validation; its response states are on Formspree.

AJAX uses `FormData` and requests JSON with an `Accept` header. Name, email,
subject, and message are the only application fields submitted. The page checks
required values and email syntax, associates inline errors with their controls,
and focuses the first invalid field. Server field errors are handled too.

While a request is pending, editing and repeat submission are blocked. Success
requires a successful HTTP response with `ok: true` and no errors. The message
says that Formspree accepted the submission; it never asserts email delivery.
An accepted draft stays visible, with sending disabled until the visitor edits
it. Network errors, invalid/unconfirmed responses, provider errors and a
25-second timeout retain all values. A `429` asks the visitor to wait. There
are no automatic retries, since a lost acknowledgement can conceal acceptance
and retrying could produce a duplicate.

Status changes use a polite, atomic live region. Errors also have visible text
and `aria-invalid`; they do not rely on color. No animation is added. The site
writes no message data to logs, cookies, local storage, or session storage.
Values exist only in the page until navigation/refresh; browser autofill and
provider-side handling are outside this JavaScript's control.

## Verification and GitHub Pages

From `website/`:

```sh
pnpm test
pnpm build
pnpm check
pnpm check:package
pnpm test:browser
```

Contact browser tests intercept the configured template in memory and use a
synthetic, blocked endpoint. They do not edit `contact.config.json` or write a
fake endpoint into production output. Tests exercise unavailable state,
required/invalid/whitespace inputs, keyboard flow, duplicate prevention,
confirmed success, server validation, rate limits, service and network errors,
malformed and contradictory success responses, timeouts, no-JavaScript POST,
and 360/1440-pixel layouts. The rest of the site suite remains in use.

The contact page is a real `contact/index.html`, with its footer link, script,
canonical URL, and sitemap entry using the existing deployment base. Test root
hosting with the same `SITE_URL=https://example.org/` environment for build,
check, and browser commands. A running preview with a different base must be
stopped first. Restore the default project-path build afterward.

No server, runtime secrets, or new packages are needed. The existing Pages
workflow includes these tests automatically and already watches `website/**`.

## Local acceptance record — 9 October 2026

- Nine unit tests passed, including three contact configuration/markup tests.
- All 23 browser tests passed at both `/My-Bruce-AI/` and `/`, including 15
  contact scenarios. Submission responses were mocked; no live messages were sent.
- Both production builds, link checks, and Pages archive checks passed: 25 HTML
  pages, 1,306 references, and 20 byte-identical allowlisted downloads.
- Contact screenshots at 360 and 1440 pixels were visually inspected. Keyboard
  order, focus, status-region attributes, and reduced-motion layout were checked
  in Chromium; a manual screen-reader session was not performed.
- Maintenance links and whitespace checks passed. Independent code review found
  one configuration-dependent test issue, which was corrected and rechecked.

The normal project-path output was restored. Live provider configuration,
recipient receipt, and a published deployment remain unverified.
