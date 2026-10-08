# Proposed changes

The patch is a reviewable **header/API sketch only** against assessed commit
`8d7313216916d8f7d554da20df1b5fb8603d0b21`. It adds two new files; it does not install
policy, refactor the USB adapter, change descriptors, initialize hardware, or complete #15.

The constructor is compiled by the focused C probe. Full policy exists only in the
synthetic Python model. Actual production wiring and backend synchronization remain to
be implemented using the refactor map. The descriptor correction is deliberately kept
out of this patch; `descriptor_check.py` evaluates it in memory only.

Patch application was checked in an isolated local tree; generated files were compared
byte-for-byte to `proposed_tree/`. Reconcile live repository state before any later use.
