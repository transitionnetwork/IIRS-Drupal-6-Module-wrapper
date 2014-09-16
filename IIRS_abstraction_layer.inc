<?php
//------------------------------------------------------------------------
//--------------------------------------------------- IIRS_common abstraction layer
//------------------------------------------------------------------------
function IIRS_0_translation($sString) {
  return "t:" . t($sString);
}

function IIRS_0_current_path() {
  return drupal_get_path_alias($_GET['q']);
}

function IIRS_0_availableLanguages() {
  return array('en', 'es', '(dev!)');
}

function IIRS_0_http_request($url, $timeout = 2.0, $var_dump_non_200 = FALSE) {
  $result = drupal_http_request($url, array(), 'GET', NULL, 0, $timeout);
  if ($result->code != 200 && $var_dump_non_200) var_dump($result);
  return ($result->code == 200 ? $result->data : '');
}

function IIRS_0_set_message($message, $IIRS_widget_mode = true) {
  if ($IIRS_widget_mode) {
    print("<div class=\"IIRS_0_message\">$message</div>");
  } else {
    drupal_set_message($message);
  }
}

//------------------------------------------------------------------------
//------------------------------------------------------- create and update user and TI
//------------------------------------------------------------------------
function IIRS_0_TI_addUser($name, $email, $pass, $phone) {
  global $language;
  $uid = NULL;

  //check to see if the user exists first
  if ($account = user_load(array('mail' => $email))) {
    $uid = $account->uid;
  } else {
    //user doesnt exist: create a new one
    //TODO: minimum password requirements: validation of user fields like username?
    if (empty($pass)) $pass = user_password(); //generates a random password
    $aFields = array(
      'name'     => $name,
      'pass'     => user_password(), //TODO: user passowrd policy?
      'mail'     => $email,
      'status'   => 1, //(variable_get('user_register', 1) == 1),
      //'realname' => $name, //TODO: user_save(real name)
    );

    if ($account = user_save('', $aFields)) {
      $uid = $account->uid;
      //notify the user with a login
      $language = $language ? $language : user_preferred_language($account);
      $mail     = _user_mail_notify('status_activated', $account->mail, $language);
    }
  }

  return $uid;
}

function IIRS_0_TI_addTI($userID, $initiative_name, $townname, $place_centre_lat, $place_centre_lng, $place_description, $place_country, $domain) {
  /* D6 TN initiative-profile as of 11/08/2014
   * Initiative Name
   * field_logo (File)
   * field_initnumber
   * field_local_initnumber
   * field_status
   * Locations (Node Locations Module Form) => what is the field name here?
   * field_region (Taxonomy)
   * field_type
   * field_communitytype
   * field_themes (Node Reference)
   * field_rel_stage (Node Reference)
   * field_members_total
   * further information
   * People & Contacts (group)
   *   field_email
   *   field_primarycontact (User Reference)
   *   field_coreteam (User Reference)
   *   field_bios (File)
   *   field_contact_media (User Reference)
   *   field_contact_volunteer (User Reference)
   *   field_webcontact_user (User Reference)
   *   field_editor_users (User Reference)
   *   field_members (User Reference)
   *
   * field_relatedprojects (Node Reference)
   * field_website_status
   * field_website_home (Link)
   * field_other_websites (Link)
   * field_news (Link)
   * field_events (Link)
   * field_mailinglist (Link)
   *
   * Taxonomy and other standard / Module fields
   */

  $capitalisedTownName = ucfirst($townname);        //ucfirst() PHP 4,5
  $capitalisedInitName = ucfirst($initiative_name); //ucfirst() PHP 4,5
  $TIStatus            = 'muller';

  //basic node save details
  $aNewNode = array(
    'title'        => $capitalisedInitName,
    'body'         => '',              //filled out later in the process
    'field_status' => array(array('value' => $TIStatus)),
  );

  //Drupal meta data
  $aNewNode['type']     = IIRS_0_CONTENT_TYPE; //TODO: dependency here
  $aNewNode['status']   = 1;                   //published
  $aNewNode['language'] = LANGUAGE_NONE;       //TODO: language of the new node

  //auto-increment initnumber if we are making an official TI
  //initnumber is required for official status
  if ($TIStatus == 'official') {
    $initNumber = IIRS_0_next_initnumber();
    $aNewNode['field_initnumber'] = array(array('value' => $initNumber));
  }

  //validate the TI
  //this includes checking for duplicates on unique fields
  node_validate($aNewNode);
  $errors = form_get_errors();
  if (!$errors) {
    //save basic node
    $oSubmittedNode      = node_submit($aNewNode);
    $oSubmittedNode->uid = $userID;
    node_save($oSubmittedNode);

    //Location Module (dependency during installation)
    //save our new / existing location
    $locations = array(array(
      'name'       => $capitalisedTownName,
      //'street'   =>
      //'additional'  =>
      'city'       => $capitalisedTownName,
      //'province' =>
      //'postal_code' =>
      'country'    => $place_country,
      'latitude'   => $place_centre_lat,    //no need to convert, Location will change it to the decimal(10,6)
      'longitude'  => $place_centre_lng,    //no need to convert, Location will change it to the decimal(10,6)
      'source'     => 3,
      'is_primary' => 0,
      'inhibit_geocode' => TRUE,            //prevent Location Module from geo-coding because we already have the coords
    ));

    $criteria = array(
      'nid'   => $oSubmittedNode->nid,
      'vid'   => $oSubmittedNode->vid,
      'genid' => IIRS_0_CONTENT_TYPE
    );
    location_save_locations($locations, $criteria);
  }

  return $errors;
}

function IIRS_0_next_initnumber() {
  //TODO: we are maxing a text field here... what if there are alphanumeric?
  //we won't be assigning initnumbers like this anyway, so leave it for the time being
  $result = db_query('select max(field_initnumber_value) as max_field_initnumber_value from {content_type_initiative_profile}');
  $row    = db_fetch_array($result);
  return intval($row['max_field_initnumber_value']) + 1;
}

function _IIRS_0_TI_translateUserFields($aValues) {
  //private function
  //change standard fields definition to Drupal TN.org fields
  $aTranslatedValues = array();
  foreach ($aValues as $key => $value) {
    //translate the standard TI field names to the Drupal TN.org ones
    if (!empty($value)) {
      switch ($key) {
        case 'name':
        {break;}
        case 'email': {
          $key = 'mail';
          break;
        }
        case 'password': {
          $key = 'pass';
          break;
        }
      }
    }
    //re-write the value
    $aTranslatedValues[$key] = $value;
  }

  return $aTranslatedValues;
}

function _IIRS_0_TI_translateTIFields($aValues) {
  //private function
  //change standard fields definition to Drupal TN.org fields
  $aTranslatedValues = array();
  foreach ($aValues as $key => $value) {
    //translate the standard TI field names to the Drupal TN.org ones
    if (!empty($value)) {
      switch ($key) {
        //Drupal meta fields
        case 'type':
        //case 'status': //there is also a CCK status field handled below
        case 'language':
        {break;}
        case 'published': {
          //there is also a CCK status field handled below
          //use published to control this Drupal state
          $key = 'status';
        }

        //base node fields title, body
        case 'name':
        case 'initiative_name':
        case 'title': {
          $key = 'title';
          break;
        }
        case 'summary': {
          $key = 'body';
          break;
        }

        //location: these fields are updated separately
        //ignore them
        case 'townname':
        case 'place_centre_lat':
        case 'place_centre_lng':
        case 'place_description':
        case 'place_country':
        {break;}

        //extended CCK field mappings
        case 'website':
        case 'domain': {
          //complex CCK field
          $key = 'field_website_home';
          $value = array(array('title' => 'main website', 'url' => $value));
          break;
        }
        case 'status': //assume this is a request to update the CCK status, not the node published state
        default: {
          //assume CCK
          $key   = "field_$key";
          $value = array(array('value' => $value));
        }
      }
    }
    //re-write the value
    $aTranslatedValues[$key] = $value;
  }

  return $aTranslatedValues;
}

function IIRS_0_TI_updateTI($aValues) {
  //using an associative array here instead because the TIs will extend their fields
  global $user;

  if (user_is_logged_in()) {
    if ($TINode = node_load(array('uid' => $user->uid, 'type' => IIRS_0_CONTENT_TYPE))) {
      $aTranslatedValues = _IIRS_0_TI_translateTIFields($aValues);
      if (count($aTranslatedValues)) {
        foreach ($aTranslatedValues as $key => $value) $TINode->$key = $value;
        node_validate($TINode);
        $errors = form_get_errors();
        if (!$errors) node_save($TINode);
      }
    }
  }
  return $errors;
}

function IIRS_0_TI_updateUser($aValues) {
  //using fixed fields here because we intend only to edit base information about users
  global $user;
  $error = FALSE;

  if (user_is_logged_in()) {
    $aTranslatedValues = _IIRS_0_TI_translateUserFields($aValues);
    if (isset($aTranslatedValues['name'])) $error = user_validate_name($aTranslatedValues['name']);
    if (!$error) {
      if (isset($aTranslatedValues['mail'])) $error = user_validate_mail($aTranslatedValues['mail']);
      if (!$error && count($aTranslatedValues)) $user = user_save($user, $aTranslatedValues);
    }
  }
  return $error;
}

//------------------------------------------------------------------------
//------------------------------------------------------- querying the TIs
//------------------------------------------------------------------------
function IIRS_0_TIs_all($page_size = 0, $page_offset = 0) {
  //use the view::load('initiatives_maps')
  //TODO: allow the IIRS_0_TIs_all() view name and field mappings to be set in the settings
  //because it includes different things to the nearby me
  //and does not apply sorting
  //in the long run (> 5000 TIs) this will be replaced by AJAX viewport loading

  $aTIs = array();

  $view = views_get_view('initiatives_maps');
  $view->set_display('page_1');
  $view->pre_execute(); //also sets up the pager
  $view->execute();

  foreach ($view->result as $TI) {
    array_push(
      $aTIs,
      array(
        'lat'      => floatval($TI->gmap_lat),
        'lng'      => floatval($TI->gmap_lon), //come on!!!! lng NOT lon
        'name'     => $TI->node_title,
        'language' => $TI->node_language,
        'status'   => $TI->node_data_field_status_field_status_value,
      )
    );
  }

  return $aTIs;
}

function IIRS_0_TIs_viewport() {
  print('IIRS_0_TIs_viewport() function not supported yet');
  exit(0);
}

function IIRS_0_TIs_nearby($centre_lat, $centre_lng, $place_description = '', $maxTIs = IIRS_0_MAX_NEARBY) {
  //use the view::load('nearby me') and set the Location: distance / proximity filter
  //TODO: programmatic $maxTIs
  //TODO: allow the IIRS_0_TIs_nearby() view name and field mappings to be set in the settings
  $aTIs = array();

  $view = views_get_view('near_me');
  $view->set_display('panel_pane_2');
  $view->pre_execute(); //also sets up the pager
  $view->execute();

  foreach ($view->result as $TI) {
    array_push(
      $aTIs,
      array(
        'lat'      => floatval($TI->gmap_lat),
        'lng'      => floatval($TI->gmap_lon), //come on!!!! lng NOT lon
        'name'     => $TI->node_title,
        'language' => $TI->node_language,
        'status'   => $TI->node_data_field_status_field_status_value,
      )
    );
  }

  return $aTIs;
}

function IIRS_0_detailsUser() {
  global $user;
  $aFields = array();

  if (user_is_logged_in()) {
    //convert to standardised fields
    $aFields = array(
      'name'  => $user->name,
      'email' => $user->mail,
    );
  }
  return $aFields;
}

function IIRS_0_detailsTI_page() {
  //TODO: IIRS_0_detailsTI_page()
  IIRS_0_set_message('IIRS_0_detailsTI_page() nto supported');
  return NULL;
}

function IIRS_0_detailsTI_user() {
  global $user;
  $aFields = array();

  if (user_is_logged_in()) {
    //specifying the content type causes the additional CCK load
    $TINode = node_load(array('uid' => $user->uid, 'type' => IIRS_0_CONTENT_TYPE));
    var_dump($TINode);
    //convert to standardised fields
    $location = $TINode->location;
    $aFields  = array(
      'name'     => $TINode->title,
      'summary'  => $TINode->body,
      'domain'   => $TINode->field_website_home[0]['url'],
      'location' => array(
        'latitude'  => $location['latitude'],
        'longitude' => $location['longitude'],
      ),
    );
  }
  return $aFields;
}

//------------------------------------------------------------------------
//------------------------------------------------------- authentication
//------------------------------------------------------------------------
function IIRS_0_logged_in() {
  return user_is_logged_in();
}

function IIRS_0_login($name, $pass) {
  global $user;
  //note that Drupal 7 returns a $uid instead
  $user = user_authenticate(array('name' => $name, 'pass' => $pass));
  return $user;
}
?>