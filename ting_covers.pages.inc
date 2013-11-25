<?php

/**
 * Return the URL for the cover of an object
 */
function ting_covers_objects() {
  $covers = array();
  $image_styles = array();
  $missing_images_local_ids = array();

  foreach ($_POST['coverData'] as $cover_info) {

    // Determine if the local id is a known negative.
    if (cache_get('ting_covers:' . $cover_info['local_id'], FALSE)) {
      break;
    }

    $path = ting_covers_object_path($cover_info['local_id']);
    if (file_exists($path)) {
      // If we we already have a valid cover image w/o style then just use it.
      $covers[] = array(
        'local_id' => $cover_info['local_id'],
        'image_style' => $cover_info['image_style'],
        'url' => image_style_url($cover_info['image_style'], $path),
      );
    }
    else {
      // Mark the image for retrieval.
      $missing_images_local_ids[] = (object) array(
        'localIdentifier' => $cover_info['local_id'],
        'libraryCode' => $cover_info['owner_id'],
        'materialType' => $cover_info['mtype'],
        'sourceName' => $cover_info['source'],
      );

      // Image style to match later one.
      $image_styles[$cover_info['local_id']] = $cover_info['image_style'];
    }
  }

  // Try to download the missing images.
  try {
    // If ting_covers_extra module is enabled, try to get extra image URLs from additional sources
    if (module_exists('ting_covers_extra')) {
      ting_covers_extra_get_image_urls(&$missing_images_local_ids, &$covers, $image_styles);
    }

    // The fallback call for remaining missing images is the ADDI service
    $service = new AdditionalInformationService(variable_get('addi_wsdl_url'), variable_get('addi_username'), variable_get('addi_group'), variable_get('addi_password'));

    // Local ids = Faust numbers. Library object identifiers can be confusing.
    $additional_informations = $service->getByLocalIdentifier($missing_images_local_ids);

    foreach ($missing_images_local_ids as $local_id) {
      // Try to extract the image url from the result.
      $source_url = FALSE;
      if (isset($additional_informations[$local_id->localIdentifier]) && $ai = $additional_informations[$local_id->localIdentifier]) {
        if ($ai->detailUrl) {
          $source_url = $ai->detailUrl;
        }
        elseif ($ai->thumbnailUrl) {
          $source_url = $ai->thumbnailUrl;
        }
      }

      // No cover image found? Pick from standard set of images
      // TODO: make as module and depend this code on it
      if (!$source_url) {
        $source_url = _ting_covers_get_default_image($local_id->materialType);
      }

      // NB: The following should now NEVER run, as we always have a source_url defined..
      // No cover image found? Cache this for future reference to avoid
      // unnecessary requests.
      if (!$source_url) {
        cache_set('ting_covers:' . $local_id->localIdentifier, 1, 'cache', $_SERVER['REQUEST_TIME'] + TING_COVERS_DEFAULT_CACHE_LIFETIME);
        continue;
      }

      // Try to download the image locally.
      if ($file = _ting_covers_pages_fetch_image(ting_covers_object_path($local_id->localIdentifier), $source_url)) {
        // Generate a path corresponding to the downloaded image, styled.
        $covers[] = array(
          'local_id' => $local_id->localIdentifier,
          'image_style' => $image_styles[$cover_info['local_id']],
          'url' => image_style_url($image_styles[$cover_info['local_id']], $file),
        );
      }
    }
  }
  catch (Exception $e) {
    watchdog('ting_covers', 'Unable to retrieve covers from ADDI: %message', array('%message' => $e->getMessage()), WATCHDOG_ERROR);
  }

  //Return all image information
  drupal_json_output($covers);
}

/**
 * Helper function to fetch and save a cover image file.
 *
 * @see image_style_create_derivative()
 *
 * @param string $filename
 *    File name, including its path within Drupal's file folder.
 * @param string $image_url
 *    URL for the source image file.
 * @return mixed
 *    A file object or FALSE on error.
 */
function _ting_covers_pages_fetch_image($filename, $image_url) {
  $result = drupal_http_request($image_url);

  //Bail if the HTTP request failed
  if ($result->code != 200) {
    return FALSE;
  }

  // Get the folder for the final location of this preset...
  $directory = dirname($filename);

  // Build the destination folder tree if it doesn't already exist.
  if (!file_prepare_directory($directory, FILE_CREATE_DIRECTORY | FILE_MODIFY_PERMISSIONS)) {
    watchdog('ting_covers', 'Failed to create directory: %directory', array('%directory' => $directory), WATCHDOG_ERROR);
    return FALSE;
  }

  return file_unmanaged_save_data($result->data, $filename, FILE_EXISTS_REPLACE);
}


/**
 * Helper function returning default image for given material types
 *
 * @param string $mtype
 *    The material type label to get standard image for.
 * @return string
 *    An URL a standard image.
 */
function _ting_covers_get_default_image($mtype) {
  switch ($mtype) {
    case 'Enkelte-Billeder':
    case 'Arkivfond':
      $source_url = $GLOBALS['base_url'] . '/sites/default/files/ting_covers_defaults/dlb_vejle.png';
      break;
    case 'Bog':
      $source_url = $GLOBALS['base_url'] . '/sites/default/files/ting_covers_defaults/bog.png';
      break;
    case 'Cd-musik':
    case 'Lydbog-cd':
    case 'Lydbog-cd-mp3':
      $source_url = $GLOBALS['base_url'] . '/sites/default/files/ting_covers_defaults/cd.png';
      break;
    case 'Node':
      $source_url = $GLOBALS['base_url'] . '/sites/default/files/ting_covers_defaults/nodepapir.png';
      break;
    case 'Avis':
    case 'Avisartikel':
      $source_url = $GLOBALS['base_url'] . '/sites/default/files/ting_covers_defaults/avis.png';
      break;
    case 'Tidsskrift':
    case 'Tidsskriftsartikel':
    case 'Artikel':
    case 'Periodikum':
      $source_url = $GLOBALS['base_url'] . '/sites/default/files/ting_covers_defaults/magasin.png';
      break;
    case 'Musik-net':
    case 'Netdokument':
    case 'Lydbog-net':
      $source_url = $GLOBALS['base_url'] . '/sites/default/files/ting_covers_defaults/globe.png';
      break;
    case 'Cd-rom':
    case 'Wii':
    case 'Playstation':
    case 'Playstation-2':
    case 'Playstation-3':
    case 'Xbox':
    case 'Xbox-360':
    case 'Dvd':
    case 'Blu-ray':
      $source_url = $GLOBALS['base_url'] . '/sites/default/files/ting_covers_defaults/dvd.png';
      break;
    case 'Spil':
    case 'Billedbog':
    case 'Tegneserie':
    case 'Video':
    case 'Lydbog-baand':
    default:
      $source_url = $GLOBALS['base_url'] . '/sites/default/files/ting_covers_defaults/standard.png';
  }

  return $source_url;
}
