# Security policy

## Supported versions

The active development state is Core 0.102 on the default branch. No security
updates or backports are promised for older states or historical documentation
packages.

## Reporting vulnerabilities

Do not publish security vulnerabilities, possible secrets, or attack details
in a public issue, pull request, or discussion.

1. In the repository view, use **Security** → **Report a vulnerability** when
   it is available.
2. If it is unavailable, contact a maintainer only through a private channel
   that they have already published. This repository does not invent an email
   address or chat channel.
3. Include a minimal reproduction, the affected version or commit, likely
   impact, and safe verification steps. Remove tokens, personal data, and
   private paths.

No response-time SLA is promised by this policy. Before public disclosure,
repository owners should enable GitHub's private vulnerability-reporting
feature or publish a reliable private reporting channel.

## Scope

Relevant areas include the build and installation path, parsers for local
input files, stored replay and export files, and GitHub Actions configuration.
Reviewing a reported problem guarantees neither an immediate fix nor a
Security Advisory publication.
