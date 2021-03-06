<?php
/**
 * @file
 *
 * Helper methods for IWPS module.
 */
/**
 * Get bout node id from term.
 */
function _iwps_get_bout_node_id_from_term($bout_term) {
  $query = new EntityFieldQuery();
  $bid = null;
  $query
    ->entityCondition('entity_type', 'node', '=')
    ->propertyCondition('type', 'scorecreep_bout', '=')
    //->fieldCondition('field_bout_type', 'field_bout_type_value', 'individual', '=')
    ->fieldCondition('field_bout', 'tid', $bout_term);


  $bouts = $query->execute();

  if ($bouts) {
      $bid = array_shift(array_keys($bouts['node']));
  }
  return $bid;
}


/**
 * Get array of users ids by bout and tournament number.
 */
function _get_uids_by_bout_number($bid, $tid) {
  $uids = array();
  $result = db_query('SELECT id.draw_number, id.uid FROM {iwps_draw} as id LEFT JOIN {iwps_bout_schematic} as ibs ON (ibs.draw_number = id.draw_number) WHERE ibs.bid = :bid AND id.tid = :tid', array(':bid' => $bid, ':tid' => $tid));
  foreach ($result as $record) {
    $uids[] = $record->uid;
  }
  return $uids;
}

/**
 * Load user ids by bout number.
 */
function _load_users_by_bout_number($bid, $tid) {
  $uids = _get_uids_by_bout_number($bid, $tid);
  /*
  $result = db_query('SELECT id.draw_number, id.uid FROM {iwps_draw} as id LEFT JOIN {iwps_bout_schematic} as ibs ON (ibs.draw_number = id.draw_number) WHERE ibs.bid = :bid AND id.tid = :tid', array(':bid' => $bid, ':tid' => $tid));
  foreach ($result as $record) {
    $uids[] = $record->uid;
  }
  */
  $users = user_load_multiple($uids);
  $weight = 0;
  foreach ($users as $user) {
    $user->iwps_weight = $weight++;
  }
  return $users;
}

/**
 * Get all opponents a user has faced in the tournament.
 */
function _get_all_opponents($uid, $tid) {

}

/**
 * Load performances in a bout.
 * TODO make rotation optional
 */
function _load_performances_in_bout($bid, $rotation, $perf = NULL) {
  $query = new EntityFieldQuery();
  $query
    ->entityCondition('entity_type', 'node')
    ->propertyCondition('type', 'individual_performance')
    ->fieldCondition('field_bout', 'tid', $bid, '=', 0)
    ->fieldCondition('field_rotation', 'value', $rotation);
  if ($perf) {
    $query->fieldCondition('field_individual_performer', 'target_id', $perf);
  }
  $result = $query->execute();
  $n = NULL;
  $perfs = array();
  if ($result) {
    foreach ( $result['node'] as $nid => $node) {
      $n = $nid;
      $perfs[] = $nid;
    }
  }
  if ($n) {
    return node_load($n);
  }
  return NULL;
}

/**
 * Get all ranks in a rotation.
 */
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
    foreach ($result['node'] as $record) {
      $pids[] = $record->nid;
    }
  }

  $performances = entity_load('node', $pids);
  $poets = array();
  foreach ($performances as $perf) {
    $poet_id = $perf->field_individual_performer[$perf->language][0]['target_id'];
    $poets[]  = $poet_id;
  }

  $poet_nodes = user_load_multiple($poets);


  foreach ($performances as $perf) {
    $poet_id = $perf->field_individual_performer[$perf->language][0]['target_id'];
    $scores[] = array(
      'rank' => 0,
      'performer' => $poet_nodes[$poet_id]->name,
      'rotation' => $perf->field_rotation['und'][0]['value'],
      'score' => $perf->field_score['und'][0]['value'],
      'bout_term' => $perf->field_bout['und'][0]['tid'],
      'uid' => $poet_id,
      'nid' => $perf->nid,
    );
  }

  // sort by score high to low, DEPENDANCY scorecreep module
  usort($scores, "array_sort_by_score");

  // set ranks
  $scores = _scorecreep_get_performance_ranks($scores);
  if ($type == 'ASSOC') {
    $assoc = array();
    for ( $i = 0; $i < count($scores); $i++) {
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
  foreach ($ranks as $uid => $rank) {
    $rank['weight'] = $original[$uid]->iwps_weight;
    $r[$uid] = $rank['rank'];
    $weight[$uid] = $original[$uid]->iwps_weight;
  }
  array_multisort($r, SORT_ASC, $weight, SORT_DESC, $ranks);
  return $ranks;
}

function _iwps_sort_bout_results(&$results) {
  for ($i = 0; $i < count($results); $i++) {
    $totalrank[$i] = $results[$i]['totalrank'];
    $totalscore[$i] = $results[$i]['totalscore'];
  }
  return array_multisort($totalrank, SORT_ASC, $totalscore, SORT_DESC, $results);
}


function _iwps_load_bouts_by_terms(&$terms) {
  $tids = array();
  foreach ($terms as $tid => $term) {
    $tids[] =  $tid;
  }
  $query = new EntityFieldQuery();
  $query
    ->entityCondition('entity_type', 'node', '=')
    ->propertyCondition('type', 'scorecreep_bout', '=')
    ->fieldCondition('field_bout', 'tid', (array)$tids, 'IN', 0);
  $arr = $query->execute();
  return  $arr['node'];
}

function _set_allscores($node) {
  $allscores = array();
  if (isset($node->field_competing_poets[$node->language])) {
    foreach ($node->field_competing_poets[$node->language] as  $uid) {
      $u = $uid['target_id'];
      $user = user_load($u);
      $allscores[$u] = array(
        'performer' => $user->name,
        'ranks' => array(),
        'scores' => array(),
        'totalrank' => NULL,
        'totalscore' => NULL,
        'totalscore' => NULL,
        'opponents' => array(),
        'strength' => NULL,
      );
    }
  }
  return $allscores;
}

function _iwps_sort_final_scores(&$allscores) {
  if (count($allscores) <= 0) {
    return $allscores;
  }

  for ($i = 0; $i < count($allscores); $i++) {
    $totalrank[$i] = $allscores[$i][9];
    $totalscore[$i] = $allscores[$i][10];
  }
  return array_multisort($totalrank, SORT_ASC, $totalscore, SORT_DESC, $allscores);
}

function _iwps_sort_all_scores($allscores, $use_strength = false) {
  if (count($allscores) <= 0) {
    return $allscores;
  }

  foreach ($allscores as $uid => $score) {
    $trank[$uid] = $score['totalrank'];
    $tstrength[$uid] = $score['strength'];
    $tscore[$uid] = $score['totalscore'];
    $allscores[$uid]['uid'] = $uid;
  }
  if ($use_strength) {
    array_multisort($trank, SORT_ASC, $tstrength, SORT_ASC, $tscore, SORT_DESC, $allscores);
  }
  else {
    array_multisort($trank, SORT_ASC, $tscore, SORT_DESC, $allscores);
  }
  return $allscores;
}

function _iwps_set_opponents_list(&$allscores, $poets_in_bout) {
  $count = count($poets_in_bout);
  for ($i = 0; $i < $count; $i++) {
    $current_user = $poets_in_bout[$i];
    for ($j = 0; $j < $count; $j++) {
      if ($poets_in_bout[$j] != $current_user) {
        $allscores[$current_user]['opponents'][] = $poets_in_bout[$j];
      }
    }
  }
}

function _iwps_set_relative_strength(&$allscores, $uid) {
  $opp_list = $allscores[$uid]['opponents'];
  $total_ranks = array();
  $count = count( $opp_list );
  for($i = 0;$i < $count; $i++) {
    $total_ranks[] = $allscores[$opp_list[$i]]['totalrank'];
  }
  if (count($total_ranks) > 0) {
    $allscores[$uid]['strength'] = number_format(array_sum($total_ranks) / count($total_ranks), 2);
  }
}

function _iwps_get_all_scores(&$node) {
  $allscores = array();
  $cat = field_get_items('node', $node, 'field_term_tournament');
  if ($cat) {
    $allscores = _set_allscores($node);
    $terms = taxonomy_get_children($cat[0]['tid']);
    ksort($terms);// this should keep all the bouts in order
    // loop through all the bouts
    foreach ($terms as $bid => $term) {
      // skip finals bouts
      if ($term->name === "Finals") {
        continue;
      }
      $bout_nid = _iwps_get_bout_node_id_from_term($bid);
      $poets_in_bout = _get_uids_by_bout_number($bout_nid, $node->nid);
      _iwps_set_opponents_list($allscores, $poets_in_bout);

      // loop through the rotations
      for ($i = 1; $i < 3; $i++) {
        $rotation = _iwps_get_ranks_in_bout_rotation($bid, $i, 'ASSOC');
        // populate the allscores
        foreach ($rotation as $uid => $ranks) {
          $allscores[$uid]['ranks'][] = $ranks['rank'];
          $allscores[$uid]['scores'][] = $ranks['score'];
        }
      }
    }
  }
  foreach ($allscores as $uid => $scores) {
    $allscores[$uid]['totalrank'] = array_sum($allscores[$uid]['ranks']);
    $allscores[$uid]['totalscore'] = array_sum($allscores[$uid]['scores']);
  }
  $use_strength = array_shift(field_get_items('node', $node, 'field_use_strength'));
  if ($use_strength['value']) {
    foreach ($allscores as $uid => $scores) {
      _iwps_set_relative_strength($allscores, $uid);
    }
  }
  return _iwps_sort_all_scores($allscores, $use_strength['value']);
}

function _iwps_generate_rows($allscores, $use_strength, $num_competitors) {
  $rows = array();
  foreach ($allscores as $uid => $score) {
    // skip performers with less than 2 ranks
    if (count($score['ranks']) < 2) {
      continue;
    }
    $row = array();
    $row[] = $score['place'];
    $row[] = $score['performer'];
    $r = $score['ranks'];
    $s = $score['scores'];
    // TODO Should be a variable set on the tourney
    for ($i = 0; $i < 4; $i++) {
      $row[] = isset($r[$i])?$r[$i]:'';
      $row[] = isset($s[$i])?$s[$i]:'';
    }
    $row[] = $score['totalrank'];
    if ($use_strength == 1) {
      if ( _iwps_tournament_complete($allscores, $num_competitors) ) {
        $row[] = $score['strength'];
      }
      else {
        $row[] = "";
      }
    }
    $row[] = $score['totalscore'];
    $rows[] = $row;
  }
  return $rows;
}

function _iwps_get_final_place($allscores) {
  $place = 0;
  $prev_rank = -1;
  $prev_strength = -1;
  $prev_score = -1;
  for ($i = 0; $i < count($allscores); $i++) {
    if ($i == 0 ||
      $prev_rank != $allscores[$i]['totalrank'] ||
      ($prev_rank == $allscores[$i]['totalrank'] && bccomp($prev_strength, $allscores[$i]['strength'], 2) !== 0) ||
      ($prev_rank == $allscores[$i]['totalrank'] && bccomp($prev_strength, $allscores[$i]['strength'], 2) == 0 && bccomp($prev_score, $allscores[$i]['totalscore'], 1) !== 0)) {
      $place = $i + 1;
    }
    $allscores[$i]['place'] = $place;

    $prev_rank = $allscores[$i]['totalrank'];
    $prev_strength = $allscores[$i]['totalscore'];
    $prev_score = $allscores[$i]['totalscore'];
  }
  return $allscores;
}

function _iwps_get_finalists($allscores, $num, $key = "place") {
  $finalists = array();
  $finalists = array_slice($allscores, 0, $num);
  while ($finalists[count($finalists) - 1][$key] === $allscores[$num][$key]) {
    $finalists[] = $allscores[$num++];
  }
  return $finalists;
}


function _iwps_tournament_complete($allscores, $num_competitors) {
  $total = 0;
  for ($i = 0; $i < count($allscores); $i++) {
    if (isset($allscores[$i]['scores']) && count($allscores[$i]['scores']) >= 4) {
      $total++;
    }
  }
  return $total >= $num_competitors;

}


