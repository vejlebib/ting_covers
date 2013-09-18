<?php

function template_preprocess_ting_object_cover(&$variables) {
  $object = $variables['object'] = $variables['elements']['#object'];
  $variables['image_style'] = $variables['elements']['#image_style'];
  $ac_source = !empty($object->reply->record['ac:source'][''][0]) ? $object->reply->record['ac:source'][''][0] : FALSE; //array_shift($object->reply->record['ac:source']);
  $dc_type = !empty($object->reply->record['dc:type']['dkdcplus:BibDK-Type'][0]) ? $object->reply->record['dc:type']['dkdcplus:BibDK-Type'][0] : $object->reply->record['dc:type'][''][0];

  //Set initial values required by the template
  $variables['classes'] = array(
    'ting-cover',
    'ting-cover-object-id-' . $object->localId,
    'ting-cover-style-' . $variables['image_style'],
    'ting-cover-owner-id-' . $object->ownerId,
    'ting-cover-mtype-' . drupal_clean_css_identifier($dc_type),
    'ting-cover-source-' . drupal_clean_css_identifier($ac_source),
  );
  $variables['image'] = '';

  //If we already have the image available locally then just use it
  $path = ting_covers_object_path($object->localId);
  if (file_exists($path)) {
    //Generate an alt tag
    $alt = implode(', ', $object->creators) . ': ' . $object->title;

    $variables['image'] = theme('image_style', array('style_name' => $variables['image_style'], 'path' => $path, 'alt' => $alt));

    //Avoid further javascript processing
    $variables['classes'][] = 'ting-cover-processed';
  }
  elseif (cache_get('ting_covers:' . $object->localId)) {
    //We know that there is no cover available for this object so avoid
    //further javascript processing
    $variables['classes'][] = 'ting-cover-processed';
  }
}

function theme_ting_object_cover($variables) {
  return '<div class="' . implode(' ', $variables['classes']) . '">' . $variables['image'] . '</div>';
}
