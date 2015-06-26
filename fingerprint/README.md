fp1020 Fingerprint HAL
------------------------
This HAL contains three components:

- fpd_client.c,h: Contains an interface for the **fingerprint_client** application. It contains raw methods
that enable/disable the fingerprint application running in the trustzone. The HAL does not explicitly use
fpd_client and instead uses the abstraction described below.
- fpd_sm.c,h: Implements a state machine that keeps the communication in one of three states: Idle, Authenticating
or Enrolling. When Idle, callees can query the available fingerprint templates, remove them, and start authentication
or enrollment of new fingerprints. When any of these two operations is started, it switches to the appropriate state.
When in that state, it will transition back to idle once the operation is completed, it times out or is explicitly
cancelled.
- fingerprint.c: The HAL implementation is a very thin wrapper around fpd_sm.
