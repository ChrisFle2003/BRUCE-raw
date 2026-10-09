import fs from 'node:fs';
const configFile = new URL('../contact.config.json', import.meta.url);
export function getContactConfig(input = JSON.parse(fs.readFileSync(configFile, 'utf8'))) {
  const endpoint = input.endpoint || '';
  // Public form IDs only. No recipient addresses, keys, query parameters or alternate hosts.
  if (endpoint && !/^https:\/\/formspree\.io\/f\/[a-zA-Z0-9]+$/.test(endpoint)) {
    throw new Error('Contact endpoint must be a public https://formspree.io/f/ form URL.');
  }
  const privacyNoticeUrl = input.privacyNoticeUrl || '';
  if (privacyNoticeUrl) {
    const url = new URL(privacyNoticeUrl);
    if (url.protocol !== 'https:' || url.username || url.password) throw new Error('Privacy notice must be a public HTTPS URL.');
  }
  const submissionMode = input.submissionMode || 'ajax';
  if (!['ajax', 'hosted'].includes(submissionMode)) throw new Error('Contact submissionMode must be ajax or hosted.');
  return { endpoint, privacyNoticeUrl, submissionMode, available: Boolean(endpoint && input.serverChecksConfigured === true) };
}
