<?php


/**
 * @file cmtracker.admin.inc
 * Provides admin forms and functions for cmtracker.
 *
 */

function cmtracker_admin_form($form, &$form_state) {

  drupal_set_title("Configuration Management Tracker - Settings");

  // Provides a configuration form for cmtracker.
  // Using custom form instead of system settings form,
  // to save configuration in our custom table.

  $form['form_ids_fieldset'] = array(
    '#type' => 'fieldset',
    '#title' => t('Select forms to track'),
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
  );

  $form_ids = array();

  // Locate all form ids with admin paths, from 'drupal_get_form' page callbacks.
  $query = db_select('menu_router', 'mr');
  $query->condition('page_callback', 'drupal_get_form');
  $query->condition('path', 'admin%', 'LIKE');
  $query->fields('mr');
  $forms = $query->execute()->fetchAll();

  foreach ($forms as $f) {
    $args = unserialize($f->page_arguments);
    if (isset($args[0])) {
      $form_id = $args[0];
      $form_ids[$form_id] = $f;

      // guess the module name
      $include_file = $f->include_file;
      //$tab_root = $f->tab_root;

      $form_id_array = explode('_', $form_id);
      $module_name = $form_id_array[0]; // default to the first part of the form

      if (NULL !== $include_file && strlen(trim($include_file)) > 0) {
        $include_array = explode('/', $include_file);
        if (strpos($include_file, 'sites/all/modules') === 0) {
          $module_name = $include_array[3];
        }
      }

      if (!array_key_exists($module_name, $form['form_ids_fieldset'])) {
        // add the fieldset if we don't have it yet
        $form['form_ids_fieldset'][$module_name] = array(
          '#type' => 'fieldset',
          '#title' => filter_xss($module_name),
          '#collapsible' => TRUE,
          '#collapsed' => TRUE,
        );
      }

      $form['form_ids_fieldset'][$module_name]['cmt_' . $form_id] = array(
        '#type' => 'checkbox',
        '#title' => filter_xss($f->title),
        '#description' => filter_xss($form_id . ' (' . $f->path . ')'),
      );
    }
  }

  drupal_alter('cmtracker_formids', $form_ids);

  // Re-order the sub-fieldsets by module name.
  ksort($form['form_ids_fieldset']);

  // Select, and disable, the cmtracker_admin_form.
  $form['form_ids_fieldset']['cmtracker']['cmtracker_admin_form']['#default_value'] = 1;
  $form['form_ids_fieldset']['cmtracker']['cmtracker_admin_form']['#disabled'] = TRUE;
  $form['form_ids_fieldset']['cmtracker']['cmtracker_admin_form']['#description'] = t('Changes made on the CM Tracker admin form (this form) are always tracked.');

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => 'Save'
  );

  /*
   * 2. checkbox: "By default config changes will be logged to the table cmtracker_log. Check this box to also log all changes to watchdog."


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

   */

  // recursion :) - track the settings for the cmtracker form!
//  $form['#submit'][] = '_cmtracker_form_submit_handler';

  return $form;

}

function cmtracker_admin_form_submit($form, &$form_state) {

  global $user;

  $selected_forms = array();
  foreach($form_state['values'] as $form_id => $val) {
    if(strlen($form_id) > 4 && substr(strtolower($form_id), 0, 4) === 'cmt_') {
      if($val == 1) {
        $selected_forms[] = substr($form_id, 4);
      }
    }
  }

  // Locate all form ids with admin paths, from 'drupal_get_form' page callbacks.
  $query = db_select('menu_router', 'mr');
  $query->condition('page_callback', 'drupal_get_form');
  $query->condition('path', 'admin%', 'LIKE');
  $query->fields('mr');
  $forms = $query->execute()->fetchAll();

  foreach ($forms as $f) {
    $args = unserialize($f->page_arguments);
    if (isset($args[0])) {
      $form_id = $args[0];
      $form_ids[$form_id]['form_details'] = $f;

      // guess the module name
      $include_file = $f->include_file;
      //$tab_root = $f->tab_root;

      $form_id_array = explode('_', $form_id);
      $module_name = $form_id_array[0]; // default to the first part of the form

      if (NULL !== $include_file && strlen(trim($include_file)) > 0) {
        $include_array = explode('/', $include_file);
        if (strpos($include_file, 'sites/all/modules') === 0) {
          $module_name = $include_array[3];
        }
      }

      $form_ids[$form_id]['module_name'] = $module_name;
    }
  }

  //@todo:
  // if the form is already enabled, don't do anything
  // if it isn't enabled, insert a record
  // finally, delete all records in cmtracker_config where the record's form_id
  // does not map to anything in $selected forms

  foreach($selected_forms as $form_id) {

    try {
      db_insert('cmtracker_config')
        ->fields(array(
            'module_name' => $form_ids[$form_id]['module_name'],
            'tracking_id' => $form_id,
            'tracking_title' => $form_ids[$form_id]['module_name'],
            'uid' => $user->uid,
            'uname' => $user->username,
            'created' => REQUEST_TIME,
            'hostname' => ip_address(),
          )
        )
        ->execute();
    } catch (PDOException $e) {
      //@todo log $e->getMessage();
    }
  }

}

function _cmtracker_admin_history($form_id) {

  // shows all tracked history for the specified $form_id
  $history = '';

  //@todo get the history for the specified $form_id

  //@todo- if the current config settings do not match the last saved config settings, flag this too!
  // if the db was tampered with behind the scenes (manually, via drush), this would expose that issue.

  // if $form_id not tracked, say so
  // if $form_id not in drupal, say that too


  return $history;

}


function _cmtracker_admin_dump($all_history = TRUE, $form_id = NULL) {

  /*
 * Note: If no changes were made since the last dump,
  CMT will still dump or email all config forms which are configured to be tracked, and their last time updated.
  This is useful in the case where a database is restored- an admin can compare the dump timestamps with the info in the database
  and can see that the last times updated are older than the last info received via email or dump file.

 */

}

function _cmtracker_admin_email($all_history = TRUE, $form_id = NULL) {

  /*
 * Note: If no changes were made since the last dump,
  CMT will still dump or email all config forms which are configured to be tracked, and their last time updated.
  This is useful in the case where a database was restored- an admin can compare the dump timestamps with the info in the database
  and can see that the last times updated are older than the last info received via email or dump file.

 */

}
