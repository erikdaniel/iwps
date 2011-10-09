<?php
/**
 * @file
 * 
 * Helper methods for IWPS module.
 */

/**
 * Load user ids by bout number.
 */
function _load_users_by_bout_number($bid) {
  $uids = array();
  $result = db_query('SELECT id.draw_number, id.uid FROM {iwps_draw} as id LEFT JOIN {iwps_bout_schematic} as ibs ON (ibs.draw_number = id.draw_number) WHERE ibs.bid = :bid', array(':bid' => $bid));
  foreach ($result as $record) {
    $uids[] = $record->uid;
  }
  $users = user_load_multiple($uids);
  $weight = 0;
  foreach ($users as $user) {
    $user->iwps_weight = $weight++;
  }
  return $users; 
}

/**
 * Load performances in a bout.
 */
function _load_performances_in_bout($bid, $rotation, $perf = NULL) {
  $query = new EntityFieldQuery();
  $query
    ->entityCondition('entity_type', 'node')
    ->propertyCondition('type', 'individual_performance')
    ->fieldCondition('field_bout', 'tid', $bid, '=', 0)
  //  ->fieldCondition('field_bout', 'tid', $bid)
    ->fieldCondition('field_rotation', 'value', $rotation);
  if ($perf) {
    $query->fieldCondition('field_individual_performer', 'uid', $perf);
  }
  $result = $query->execute();
  $n = NULL;
  $perfs = array();
  if ($result) {
    foreach( $result['node'] as $nid => $node) {
      $n = $nid;
      $perfs[] = $nid; 
    }
  }
  if ($n) {
    return node_load($n);
  }
  return NULL;
}

function _iwps_get_ranks_in_bout_rotation($bid, $rotation, $type = 'array') {
  $scores = array();
  $query = new EntityFieldQuery();

  $query
    ->entityCondition('entity_type', 'node', '=')
    ->propertyCondition('type', 'individual_performance', '=')
    ->fieldCondition('field_rotation', 'value', $rotation)
    ->fieldCondition('field_bout', 'tid', $bid, '=', 0);
   
  $result = $query->execute();

  $pids = array();

  if ( array_key_exists('node', $result) ) {
    foreach($result['node'] as $record) {
      $pids[] = $record->nid;
    }
  }

  $performances = entity_load('node', $pids);   
  $poets = array();
  foreach ($performances as $perf) {
    //dvm($perf);
    $poet_id = $perf->field_individual_performer[$perf->language][0]['uid'];
    $poets[]  = $poet_id;
  }

  $poet_nodes = user_load_multiple($poets);

  
  foreach ($performances as $perf) {
    $poet_id = $perf->field_individual_performer[$perf->language][0]['uid'];
    $scores[] = array(
      'rank' => 0,
      'performer' => $poet_nodes[$poet_id]->name,
      'rotation' => $perf->field_rotation['und'][0]['value'],
      'score' => $perf->field_score['und'][0]['value'],
      'bout_term' => $perf->field_bout['und'][0]['tid'],
      'uid' => $poet_id,
    );
  }

  // sort by score high to low, DEPENDANCY scorecreep module
  usort($scores, "array_sort_by_score");
  // set ranks
  $scores = _scorecreep_get_performance_ranks($scores);
  if ($type == 'ASSOC') {
    $assoc = array();
    for( $i = 0; $i < count($scores); $i++) {
      $assoc[$scores[$i]['uid']] = $scores[$i];
    }
    return $assoc;
  }
  return $scores;
}

/**
 * A method to re-sort a set of performers for the 
 * second rotation of the tournament.  If there is a 
 * tie for a rank, the order of the tied poets is the
 * reverse of the previous rotation.
 * 
 * @param {Object} $ranks An array of the ranks in the rotation
 * @param {Object} $original An array of users in order by first rotation.
 */
function _iwps_sort_rotation($ranks, $original) {
  $orig_set = array();
  $rank_set = array();
  $count = 0;
  foreach($ranks as $uid => $rank) {
    $rank['weight'] = $original[$uid]->iwps_weight;
    $r[$uid] = $rank['rank'];
    $weight[$uid] = $original[$uid]->iwps_weight;
  }
  array_multisort($r, SORT_ASC, $weight, SORT_DESC, $ranks);
  return $ranks;
}
