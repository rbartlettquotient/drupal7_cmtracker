CMT - Config Mgmt Tracker - aka Cover My Tush

Allows those with the 'administer cmt' permission to configure tracking of the values of any admin/config settings form.

The module simply adds a custom submit handler in hook_form_alter, for those config forms that should be tracked.
The submit handler logs the values that were saved, formatted in JSON, in the cmt_log table.

The admin UI allows configuring CM tracking.

UI provides:

1. list of checkboxes for anything defined as an admin/config form. Admins check anything they want tracked.

2. checkbox: "By default config changes will be logged to the table cmt_log. Check this box to also log all changes to watchdog."


3. fieldset for emailing captures
radios: email capture data periodically (never, weekly, daily, hourly)
textbox: email recent config changes to a text file - enter location here - allow tokens (private or public filesystem location)
Note: only changes made since the last dump will be dumped.

Button: email all config history now (note: this make take a long time. There are N config changes in cmt_log).


4. fieldset for offline dumps of captures
radios: dump capture data periodically (never, weekly, daily, hourly)
textbox: dump recent config changes to these addresses:
Note: only changes made since the last email will be sent.

Button: dump all config history to file now (note: this make take a long time. There are N config changes in cmt_log).


Note: If no changes were made since the last dump,
CMT will still dump or email all config forms which are configured to be tracked, and their last time updated.
This is useful in the case where a database was restored- the admin will see that the last times updated are
older than the last info received via email or dump file.

