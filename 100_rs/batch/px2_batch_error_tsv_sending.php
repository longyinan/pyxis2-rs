<?php

// エラー時保存先
define( 'TSV_ERROR_DIR',   '/data/error_tsv/' );
// 送信対象保存先
define( 'TSV_BATCH_DIR',   '/data/batch_tsv/' );
// 送信対象保存先
define( 'TSV_BATCH_SEND_DIR',   '/data/batch_tsv/request/' );
// 送信失敗ファイル保存先
define( 'TSV_BATCH_ERROR_DIR',   '/data/batch_tsv/error/' );

// 認証キー文字列
define( 'KEY_STRING',  'htru3zjv' );
// 配信管理システムAPIのURL
define( 'TSV_TRANSFER_URL',  'https://api.mother.cross-m.co.jp/api/noticeRecordMd/' );

define( 'ERR_REPORT_USE',  TRUE );
define( 'ERR_REPORT_ADDR_FROM',  'system_admin@cross-m.co.jp' );
define( 'ERR_REPORT_ADDR_TO',  'syss_report@ml.cross-m.co.jp' );

define('FILE_PATTERN', '*.tsv');
define('RETRY_TIMES', 3);
define('REQUEST_FILE_ROWS', 500);
define('TSV_SEND_TERM', 24);

define( "LOG_FILE", "/var/log/px2_batch_error_tsv_sending.log" );

$subjects = array('part_no', 'panel_id', 'mtid', 'monitor_enquete_end_flg', 'answer_date', 
	'quota1', 'quota2', 'quota3', 'user_agent');

// １、エラーのTSVファイルを送信フォルダーへ移動
$errorfiles = glob(TSV_ERROR_DIR . FILE_PATTERN); 

// ディレクトリ初期化
if (! file_exists(TSV_BATCH_SEND_DIR)) {
	mkdir(TSV_BATCH_SEND_DIR, 0755, TRUE);
}
if (! file_exists(TSV_BATCH_ERROR_DIR)) {
	mkdir(TSV_BATCH_ERROR_DIR, 0755, TRUE);
}

foreach ($errorfiles as $errorfile) {
	$tmperrorfile = TSV_BATCH_DIR . uniqid(date('Ymd')) . '.tsv';
	$times = 0;
	while($times < RETRY_TIMES) {
		if (rename($errorfile, $tmperrorfile)) {
			break;
		}
		$times++;
	}

	//TODO 失敗の場合
	if ($times == RETRY_TIMES ) {

	}

}

// ２、送信ファイル作成（500件ずつ）
$errorfiles = glob(TSV_BATCH_DIR . FILE_PATTERN); 
$failedrecords = array();

foreach ($errorfiles as $errorfile) {
	$fin = fopen($errorfile, 'r');
	$sendfile = newRequestFileName();
	$fout = fopen($sendfile, 'w');

	$row = 0;
	while(($line = fgetcsv($fin, 0, "\t")) !== FALSE) {

		// 予定の行数を超えた場合、新しいファイルを生成する
		if ($row >= REQUEST_FILE_ROWS) {
			fclose($fout);

			$sendfile = newRequestFileName();
			$fout = fopen($sendfile, 'w');

			$row = 0;
		}		

		// 1行目の場合、タイトル行出力
		if ($row == 0) {
			fputcsv($fout, $subjects, "\t");
		}

		// 失敗の場合
		$outline = implode("\t", $line);
		if (fputs($fout, $outline . PHP_EOL) === FALSE) {
			$failedrecords[] = $outline;
		}

		$row ++;

	}

	fclose($fin);
	// 処理終了後、ファイル削除
	unlink($errorfile);
}

//　３、送信
$sendfiles = glob(TSV_BATCH_SEND_DIR . FILE_PATTERN); 
$failedinfo = array();
$failedfiles = array();

foreach ($sendfiles as $sendfile) {

	$result = tsv_postTsvFile($sendfile, $afile);
	// 成功の場合、ファイル削除
	if ($result === TRUE) {
		unlink($afile);
	// 500番台エラーの場合、再送
	} elseif(isset($result['info']['http_code']) && preg_match("/^5/", $result['info']['http_code'])) {
//		error_log('500番台エラーは起きました。エラーのファイルは次のバッチ処理で再送します。[' . $afile . ']');
		tsv_writeLog('500番台エラーは起きました。エラーのファイルは次のバッチ処理で再送します。[' . $afile . ']');

	// その他のエラーの場合
	} else {
		$failedinfo[] = $result;

		$faildfile = basename($afile);
		rename($afile, TSV_BATCH_ERROR_DIR . $faildfile);
		$failedfiles[] = $faildfile;

	}
}

//　４、エラーがあった場合、メール通知
$mail_message = '';
if (count($failedinfo) > 0) {
	$mail_message .= '配信管理システム（カバ）へ送信処理に以下のエラーが発生しました。' . "\n";
	foreach ($failedinfo as $info) {
		$mail_message .= "[謝礼リクエストID]" . $info['p2_record_request_id'] . "\n";
		$mail_message .= "[リストファイル名]" . $info['filename'] . "\n";
		// $mail_message .= json_encode($info). "\n";
		foreach ($info['errors'] as $error) {
			$mail_message .= "[エラーコード]" . $error['code'] . "\n";
			$mail_message .= "[エラーメッセージ]" . $error['message'] . "\n";
		}
		$mail_message .= "\n";
	}

//	tsv_faildAlert($mail_message);
}

if (count($failedrecords) > 0) {
	$mail_message .= "\n";
	$mail_message .= "以下のレコードはファイルに書き込みに失敗しまして、送信できませんでした。". "\n";
	foreach ($failedrecords as $record) {
		$mail_message .= implode("\t", $record) . "\n";
	}

//	tsv_faildAlert($mail_message);
}


// ５、送信エラーファイルクリーン
$sendfiles = glob(TSV_BATCH_SEND_DIR . FILE_PATTERN); 
$nowdt = new DateTime(); 

foreach ($sendfiles as $sendfile) {

	$filedt = new DateTime(date('YmdHis', filemtime($sendfile)));
	$interval = date_diff($filedt, $nowdt);

	// ２４時間以内送信出来なかった場合、Errorフォルダーに移動
	if ($interval->format('h') >= TSV_SEND_TERM) {
		$faildfile = basename($sendfile);
		rename($sendfile, TSV_BATCH_ERROR_DIR . $faildfile);
		$failedfiles[] = $faildfile;
	}
}

//　６、送信失敗ファイルある場合、メール通知
if (count($failedfiles) > 0) {
	$mail_message .= "\n" . '以下ファイルは配信管理システム（カバ）へ送信できませんでした。' . "\n";
	foreach ($failedfiles as $failedfile) {
		$mail_message .= '  '. $failedfile . "\n";
	}

	$mail_message .= '保存場所：' . TSV_BATCH_ERROR_DIR;
}

// ７、エラーある場合、メール通知
if (strlen($mail_message) > 0 ) {
	tsv_faildAlert($mail_message);
	tsv_writeLog($mail_message);
}

die();


/**
 * 新しいリクエストファイル名を生成する
 */
function newRequestFileName() {

	$sendfile = TSV_BATCH_SEND_DIR . uniqid(date('YmdHis')) . '.tsv';
	return $sendfile;

}

/* ****************************************************
// TSVファイルを配信管理システムにポストする。
// 引数: $filepath=回答情報ファイル名
// 引数： $newfile=リネーム後のファイル名
*/
function tsv_postTsvFile($filepath, &$newfile){
	// TSVのパスがない場合処理終了
	if(!$filepath) return false;

	$processDateTime = new DateTime();
	$tsvCreateDate = $processDateTime->format('Ymd');
	$idString = $processDateTime->format('YmdHis') . substr(microtime(),2,6);
	$postRequestDate = $processDateTime->format('Y-m-d H:i:s');
	$errorTsvDateString = $processDateTime->format('YmdH');

	// 必要なパラメーターの作成
	$auth_key = hash('sha256', KEY_STRING . '-' . $postRequestDate);

	// 実際のファイル名をlist_file_nmに合わせる
	$afilename = 'recordapi_' . $tsvCreateDate . '_' . $idString . '.tsv';
	$afile = str_replace(basename($filepath), $afilename, $filepath);
	if (rename($filepath, $afile)) {
		$newfile = $afile;
	} else {
		$newfile = $filepath;
	}

	// 件数（タイトル行を除く）
	$rowCount = count(file($afile)) - 1;

	// リクエストの作成
	$postfields = array(
		'sysid'		=> 'p2',
		'auth_key'	=> $auth_key,
		'request_date'	=> $postRequestDate,
		'p2_record_request_id'	=> $idString,
		'list_file_nm'		=> $afilename,
		'list_data_count'	=> $rowCount,
		'a_file'		=> '@'. $afile,
	);

	//リクエストの実行
	$responce = call_hoppo_api( TSV_TRANSFER_URL, $postfields);

	return $responce['success'] ? $responce['success'] : 
		array('p2_record_request_id' => $idString, 'filename' => $afilename, 
			'errors' => $responce['contents'], "info" => $responce['info']);
}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
/*
 * Function   : call_hoppo_api
 *
 * 概要       : cURL関数を使用して簡単にリクエストを行うための関数
 * 備考       : 戻り値の内容は以下の配列となる。
 *                Array(
 *                  'success'  => 処理結果 (TRUEまたはFALSE)
 *                  'contents' => 応答内容
 *                  'info'     => array(
 *                                  curl_getinfoの結果。各項目の詳細は以下を参照。
 *                                  http://www.php.net/manual/ja/function.curl-getinfo.php
 *                                )
 *                  'errno'    => cURLのエラー番号。詳細は以下を参照
 *                                http://www.php.net/manual/ja/function.curl-errno.php
 *                  'error'    => cURLのエラー内容を示す文字列。
 *                )
 *              ※尚、パラメーターにファイルを指定する場合には'@'+ファイルのパスを指定すること。
 *
 * @param String $url リクエスト先のURL
 * @param Array $data リクエストパラメーターの配列 (パラメーター名をキーに指定)
 * @param Boolean $is_post POSTで送信するか (FALSEにした場合はGETで送信)
 * @param Int $connect_timeout 接続のタイムアウト (デフォルトは10秒)
 * @param Int $timeout 応答のタイムアウト (デフォルトは300秒)
 * @return Array 応答に関する情報の配列
*/
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
function call_hoppo_api( $url, $data, $is_post = TRUE, $connect_timeout = 10, $timeout = 3600 ) {

	$ch = curl_init();

	if ( $is_post ) {
		// POSTの場合にはCURLOPT_POSTFIELDSでパラメーターを設定。
		curl_setopt( $ch, CURLOPT_POST, TRUE );
		curl_setopt( $ch, CURLOPT_POSTFIELDS, $data );
	} else {
		// GETの場合にはURLにクエリ文字列として付加。
		$params = array();
		foreach ( $data as $name => $value ) {
			$params[] = $name . "=" . $value;
		}
		$querystr = implode( "&", $params );
		if ( $params ) {
			$url .= "?" . $querystr;
		}
	}

	curl_setopt( $ch, CURLOPT_URL, $url );
	curl_setopt( $ch, CURLOPT_CONNECTTIMEOUT, $connect_timeout );
	curl_setopt( $ch, CURLOPT_TIMEOUT, $timeout );
	curl_setopt( $ch, CURLOPT_RETURNTRANSFER, TRUE );

	// SSL証明書を検証しない
	curl_setopt( $ch, CURLOPT_SSL_VERIFYPEER, FALSE );
	curl_setopt( $ch, CURLOPT_SSL_VERIFYHOST, FALSE );
	// cURLの処理について詳細を標準出力に出力します
	curl_setopt( $ch, CURLOPT_VERBOSE, FALSE );

	// 他のオプションを設定する場合はこちらで
	// 詳しくは以下のURLを参照
	// http://www.php.net/manual/ja/function.curl-setopt.php

	$contents = curl_exec( $ch );
	$info     = curl_getinfo( $ch );
	$errno    = curl_errno( $ch );
	$error    = curl_error( $ch );

	curl_close( $ch );
	$ct =json_decode($contents, true);
	if (isset($ct['errors'])) {
		$ct = $ct['errors'];
	}

//	$success = ( $contents !== FALSE );
	$success = ($info['http_code'] == 200);

	return array( "success" => $success, 
		"contents" => $ct, 
		"info" => $info, 
		"errno" => $errno, 
		"error" => $error );

}


/* ****************************************************
// エラーファイル作成失敗時のメール通知を行う
*/
function tsv_faildAlert($errorinfo){

	// メールメッセージ作成
	$mail_title = 'Pyxis2-配信管理システム リカバリバッチエラー検知';

	$mail_message = "リカバリバッチ処理に失敗しました。\n\n";
	$mail_message .= "----------------------------------\n";

	$mail_message .= $errorinfo;

	// 送り先にメール
	if ( ERR_REPORT_USE ) {
	// メール送信実行
		// mb_send_mail( STOP_ERR_REPORT_ADDR_TO, $mail_title, $mail_message, 'From:' . STOP_ERR_REPORT_ADDR_FROM, '-f ' . STOP_ERR_REPORT_ADDR_FROM );
		mb_send_mail( ERR_REPORT_ADDR_TO, $mail_title, $mail_message, 'From:' . ERR_REPORT_ADDR_FROM, '-f ' . ERR_REPORT_ADDR_FROM );
//		error_log($mail_message);
	}
}

	//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
	/*
	 * Function   : tsv_writeLog
	 *
	 * 概要       : ログファイルに処理内容を出力する
	 * 備考       : 直接コマンドラインから叩かれた場合を想定してメッセージを標準出力
	*/
	//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
	function tsv_writeLog( $log_msg, $write_argv = array() ) {

		// ファイルオープン（追記モード）
		if ( !$fp = fopen( LOG_FILE, 'a+' ) ) {
			return false;
		}

		// set_file_buffer( $fp, 0 );

		// ファイルロック
		if ( flock( $fp, LOCK_EX ) ) {

			// 処理実行時間を取得
			list( $msec, $sec ) = explode( " ", microtime() );

			// バッファリング開始
			ob_start();

			print( date( "Y-m-d H:i:s", time() ) . " " . $msec . "\t" . $log_msg . "\n" );

			if ( $write_argv ) {
				print_r( $write_argv );
			}

			// バッファ出力
			$buffer = ob_get_contents();

			// バッファリング終了
			ob_end_clean();

			// メッセージ書き込み
			fwrite( $fp, $buffer );

			// ロック解除
			flock( $fp, LOCK_UN );
		}

		// ファイルクローズ
		fclose( $fp );

		return;
	}

