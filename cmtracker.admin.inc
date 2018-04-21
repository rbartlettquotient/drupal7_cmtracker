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

  $currently_selected = db_select('cmtracker_config', 'cc')->fields('cc')->execute()->fetchAllAssoc('tracking_id');

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
        '#default_value' => isset($currently_selected[$form_id]) ? true : false,
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
   * 2. checkbox: "By default config changes will be logged to the table cmtracker_log.
   * Check this box to also log all changes to watchdog."


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

  $currently_selected = db_select('cmtracker_config', 'cc')->fields('cc')->execute()->fetchAllAssoc('tracking_id');

  foreach($selected_forms as $form_id) {

    // if the form is already enabled, don't do anything
    // if it isn't enabled, insert a record
    if(!array_key_exists($form_id, $currently_selected)) {
      try {
        db_insert('cmtracker_config')
          ->fields(array(
              'module_name' => $form_ids[$form_id]['module_name'],
              'tracking_id' => $form_id,
              'tracking_title' => $form_ids[$form_id]['module_name'],
              'uid' => $user->uid,
              'uname' => $user->name,
              'created' => REQUEST_TIME,
              'hostname' => ip_address(),
            )
          )
          ->execute();

        db_insert('cmtracker_log')
          ->fields(array(
              'module_name' => $form_ids[$form_id]['module_name'],
              'tracking_id' => $form_id,
              'tracking_title' => $form_ids[$form_id]['module_name'],
              'cmt_value' => 'Form tracking was enabled.',
              'uid' => $user->uid,
              'uname' => $user->name,
              'created' => REQUEST_TIME,
              'hostname' => ip_address(),
            )
          )
          ->execute();

      } catch (PDOException $e) {
        //@todo log $e->getMessage();
      }
    } // If this form is not being tracked yet.
  } // Each form selected in the UI.

  // finally, delete all records in cmtracker_config
  // where the record's form_id
  // does not map to anything in $selected forms
  foreach($currently_selected as $tracking_id => $config_record) {
    if(!in_array($tracking_id, $selected_forms)) {
      db_delete('cmtracker_config')
        ->condition('tracking_id', $tracking_id)
        ->execute();

      $module_name = isset($form_ids[$tracking_id]['module_name']) ? $form_ids[$tracking_id]['module_name'] : '';
      db_insert('cmtracker_log')
        ->fields(array(
            'module_name' => $module_name,
            'tracking_id' => $tracking_id,
            'tracking_title' => $module_name,
            'cmt_value' => 'Form tracking was disabled.',
            'uid' => $user->uid,
            'uname' => $user->name,
            'created' => REQUEST_TIME,
            'hostname' => ip_address(),
          )
        )
        ->execute();

    }
  }

}

function _cmtracker_admin_history($form, &$form_state, $form_id) {

  // Shows all tracked history for the specified $form_id
  $form = array();

  // if $form_id not tracked, say so
  $currently_selected = db_select('cmtracker_config', 'cc')->fields('cc')->execute()->fetchAllAssoc('tracking_id');
  if(!array_key_exists($form_id, $currently_selected)) {
    drupal_set_message('The specified form ID (' . $form_id . ') is not being tracked by CM Tracker.', 'warning');
    return array();
  }

  //@todo if $form_id not in drupal, say that too

  $sel = db_select('cmtracker_log', 'cl')
    ->fields('cl', array(
        'cmtracker_log_id',
        'form_path',
        'uname',
        'hostname',
      )
    )
    ->condition('tracking_id', $form_id)
    ->orderBy('form_path', 'ASC')
    ->orderBy('created', 'DESC');
  $sel->addExpression('FROM_UNIXTIME(created)', 'created');

  $config_saves = $sel->execute()->fetchAllAssoc('cmtracker_log_id');

  if(count($config_saves) > 0) {
    //@todo- if the current config settings for the form do not match the last saved config settings, flag this too!
    // if the db was tampered with behind the scenes (manually, via drush, whatever), this would expose that issue.
    // Only do this if the form is actively being tracked,
    // and if there is at least one entry saved *after/since* the log entry that was added when we started tracking this form.

    // Table header
    $header = array(

      'created' => array('data' => 'created', 'field' => t('Log Date')),
      'path' => array('data' => 'path', 'field' => t('Path')),
      'user' => array('data' => 'user', 'field' => t('User')),
      'hostname' => array('data' => 'hostname', 'field' => t('Hostname')),
      //'diff_link' => array('data' => t('View Differences'), 'field' => 'diff_link'),
      'operations' => array('data' => t('Operations'), 'field' => 'operations'),
    );

    $options = array();
    $i = 1;
    $count_saves = count($config_saves);

    $config_saves_keys = array_keys($config_saves);
    foreach($config_saves as $id => $entry) {
      if($i < $count_saves) {
        $prev_entry_created = $config_saves_keys[$i];
        $prev_entry = $config_saves[$prev_entry_created];
        $link = '/admin/config/system/cmtracker/diff/'
          . $entry->cmtracker_log_id . '/' . $prev_entry->cmtracker_log_id;
      }
      else {
        $link = '';
      }

      $operations = array(
        'edit' => array(
          'title' => t('edit'),
          'href' => '/blah',
          'query' => 'blah',
        ),
        'delete' => array(
          'title' => t('delete'),
          'href' => '/blah',
          'query' => 'blah',
        ),
        /*        'delete' => array(
          'title' => t('delete'),
          'href' => '/admin/content/object-group-delete/' . $og_id, // TODO: Add permissions
          'query' => $destination,
        ),
*/
      );

      $options[$id] = array(
        'operations' => array(
          'data' => array(
            '#theme' => 'links',
            '#links' => array(
              'diff' => array(
                'title' => 'view diff',
                'query' => '',
                'href' => $link,
              ),
            ),
            '#attributes' => array(
              'class' => array('links', 'inline')
            ),
          ),
        ),
        'created' => $entry->created,
        'path' => $entry->form_path,
        'user' => $entry->uname,
        'hostname' => $entry->hostname,
      );
      $i++;
    }

    dpm($options);

    // Add table
    $form['cmt_log_entries'] = array(
      '#type' => 'tableselect',
      '#header' => $header,
      '#options' => $options,
      '#empty' => t('No log entries available.'),
      '#js_select' => FALSE,
      '#attributes' => array(),
    );

    # custom pager
    //@todo
    /*
    $total_count = count($config_saves);
    $form['pager'] = array(
      '#type' => 'markup',
      '#markup' => ogmt_list_pager($total_count, $rows, $start),
    );
    */

  } // If we have some log entries.

  return $form;

}

function _cmtracker_admin_diff($prev_id, $next_id) {

  $sel = db_select('cmtracker_log', 'cl')
    ->fields('cl')
    ->condition('cmtracker_log_id', array($prev_id, $next_id), 'IN');
  $sel->addExpression('FROM_UNIXTIME(created)', 'created_nice');
  $config_saves = $sel->execute()->fetchAllAssoc('cmtracker_log_id');

  $prev = NULL;
  $next = NULL;

  if(array_key_exists($prev_id, $config_saves)) {
    $prev = $config_saves[$prev_id];
  }
  if(array_key_exists($next_id, $config_saves)) {
    $next = $config_saves[$next_id];
  }

  if(NULL == $prev_id || NULL == $next_id) {
    drupal_set_message('One or both of the configuration log entries could not be found.', 'error');
    return array();
  }

  $prev_json = json_encode($prev->cmt_value);
  $next_json = json_encode($next->cmt_value);

  $compare = strcmp($prev_json, $next_json);
  if(0 !== $compare) {
    drupal_set_message('Values do not match!', 'error');

  }
  else {
    drupal_set_message('Values match.');
  }

  $prev_link = l('View ' . $prev->created_nice, '/admin/config/system/cmtracker/view/' . $prev_id);
  $next_link = l('View ' . $next->created_nice, '/admin/config/system/cmtracker/view/' . $next_id);

  $render_array = array(
    'tracking_id' => array(
      '#type' => 'markup',
      '#markup' => '<h1>Config log entry for ' . $prev->tracking_id . '</h1>'
    ),
    'prev_link' => array(
      '#type' => 'markup',
      '#markup' => '<p>' . $prev_link . '</p>'
    ),
    'next_link' => array(
      '#type' => 'markup',
      '#markup' => '<p>' . $next_link . '</p>'
    )
  );

  return drupal_render($render_array);

}

function _cmtracker_admin_view($config_id) {

  $sel = db_select('cmtracker_log', 'cl')
    ->fields('cl')
    ->condition('cmtracker_log_id', $config_id);
  $sel->addExpression('FROM_UNIXTIME(created)', 'created_nice');
  $config_saves = $sel->execute()->fetchAllAssoc('cmtracker_log_id');

  $log = $config_saves[$config_id];

  $json_log = json_encode($log->cmt_value);

  $render_array = array(
    'tracking_id' => array(
      '#type' => 'markup',
      '#markup' => '<h1>Config log entry for ' . $log->tracking_id . '</h1>'
    ),
    'created' => array(
      '#type' => 'markup',
      '#markup' => '<h2>' . $log->created_nice . '</h2>'
    ),
    'path' => array(
      '#type' => 'markup',
      '#markup' => '<h3>Form path: ' . $log->form_path . '</h3>'
    ),
    'json_log' => array(
      '#type' => 'markup',
      '#markup' => '<pre>' . $json_log . '</pre>'
    )
  );

  return drupal_render($render_array);

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
