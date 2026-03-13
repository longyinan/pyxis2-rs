<?php

/******************************************************************************
 px2_batch_rs_tbl_cleaning.php
 
 実査サーバーの回答テーブルお掃除スクリプト
 ******************************************************************************/


require_once(dirname(__FILE__) . '/../htdocs/common/server_const.inc');
define('PX2_CLEANING_LOGNAME', PX2_CLEANING_LOG_PATH . '/' . PX2_CLEANING_LOGNAME_RS_TBL);
require_once(dirname(__FILE__) . '/px2_batch_common_cleaning.inc');


$start = time();
$cleaning_target_list = NULL;
$result = NULL;
$mail_params = array();


$mail_params['subject'] = PX2_CLEANING_MAIL_SUBJECT_RS_TBL;
$mail_params['from'] = PX2_CLEANING_MAIL_FROM_RS_TBL; 
$mail_params['to'] = PX2_CLEANING_MAIL_TO_RS_TBL;
// $mail_params['server'] = getenv('HOSTNAME');
// cronから実行すると環境変数が取得出来ないのでphp_uname()でホスト名を取得
$mail_params['server'] = php_uname( "n" );
$mail_params['title'] = PX2_CLEANING_MAIL_TITLE_RS_TBL;
$mail_params['script'] = __FILE__;
$mail_params['log'] = PX2_CLEANING_LOGNAME;


// 回答テーブルのお掃除はアンケート個別の設定とは関係なくPX2_RAW_DATA_ALIVEで指定された日数で行う
$cleaning_target_list = get_cleaning_target_list(DEVELOP_DB_CONNECT_STRING, FALSE, PX2_CLEANING_RAW_DATA_ALIVE);

if ($cleaning_target_list === FALSE) {
	// お掃除対象のリストを取得できなかった
	$mail_params['target_error'] = TRUE;
} else {
	// お掃除対象のリストを取得できた
	$mail_params['target_error'] = FALSE;
	// お掃除を実行
	$result = cleaning_rs_tables(DB_CONNECT_STRING, $cleaning_target_list);
	$mail_params['data_success'] = count($result['success_list']);
	$mail_params['data_error'] = count($result['error_list']);
}


$finish = time();
$mail_params['start'] = $start;
$mail_params['finish'] = $finish;
$mail_params['date'] = $finish;


// メール送信
send_reportmail($mail_params);


exit;

