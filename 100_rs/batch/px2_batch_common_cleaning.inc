<?php

/******************************************************************************
 px2_batch_common_cleaning.inc
 
 お掃除スクリプトの共通処理
 ******************************************************************************/


mb_language("Ja");
mb_internal_encoding("UTF-8");


// 動作確認の際にデータやソースの削除を実際に行わない場合に設定する
// 0: 通常通り実行する。
// 1: ソース削除は行わない。データ削除はSQLを実行するがロールバックする。
// 2: ソース削除は行わない。データ削除はSQLは実行しないでSQLをログに残すだけにする。


// コマンドライン引数からの設定
if ($argc > 1) {
	$dryrun = (int)$argv[1];
	if (is_numeric($argv[1]) and in_array($dryrun, array(0,1,2))) {
		define('PX2_CLEANING_DRYRUN', $dryrun);
	} else {
		echo "Invalid arguments. (PX2_CLEANING_DRYRUN [0-2])" . PHP_EOL;
		exit;
	}
}
if ($argc > 2) {
	$basedate = $argv[2];
	if ($basedate = strtotime($basedate)) {
		define('PX2_CLEANING_BASEDATE', $basedate);
	} else {
		echo "Invalid format. (PX2_CLEANING_BASEDATE)" . PHP_EOL;
		exit;
	}
}


// デフォルトの実行レベル
if (! defined('PX2_CLEANING_DRYRUN')) {
	define('PX2_CLEANING_DRYRUN', 0);
}
// デフォルトの基準日時 (現在の日時)
if (! defined('PX2_CLEANING_BASEDATE')) {
	define('PX2_CLEANING_BASEDATE', date('Y-m-d H:i:s'));
}


// 実行モードを表示
// echo "PX2_CLEANING_DRYRUN: " . PX2_CLEANING_DRYRUN . PHP_EOL;
// echo "PX2_CLEANING_BASEDATE: " . date('Y-m-d H:i:s', PX2_CLEANING_BASEDATE) . PHP_EOL;



/*----------------------------------------------------------------------------*
 お掃除対象のリストを返す
 $db_connstr	接続先のDB接続文字列 (px2_db10またはpx2_db50になる)
 $with_eqt	パートNoも取得する場合はTRUE
 $day_alive	保持期間 (指定無しの場合はend_page_switch_dateを用いる)
 *----------------------------------------------------------------------------*/
function get_cleaning_target_list($db_connstr, $with_eqt=TRUE, $day_alive=NULL) {
	// お掃除対象のリスト
	$cleaning_target_list = array();

	// DBに接続
	$db = pg_connect($db_connstr);

	$sql = '';
	if ($with_eqt) {
		$sql .= "SELECT pi.nxs_ank_book_seq, ei.nxs_enquete_no, pi.upd_ymd_hms ";
	} else {
		$sql .= "SELECT pi.nxs_ank_book_seq, pi.upd_ymd_hms ";
	}
	$sql .= "FROM pyxis2_sys.prj_info pi ";
	if ($with_eqt) {
		// パートNoも必要な場合はeqt_infoとJOINして取得する
		$sql .= "JOIN pyxis2_sys.eqt_info ei ON (pi.nxs_ank_book_seq = ei.nxs_ank_book_seq) ";
	}

	// 完了ステータスのアンケートを対象とする
	$sql .= "WHERE pi.questionnaire_state = '9' ";

	// 完了ステータスになった後は最終更新年月日が変わらないという前提で完了後の経過日数を求める
	$sql .= "AND date_part('day', '" . PX2_CLEANING_BASEDATE . "'::timestamp - pi.upd_ymd_hms::timestamp) ";
	if ($day_alive) {
		// $day_aliveが指定されている場合
		// 経過日数が{$day_alive}日以上{$day_alive + PX2_CLEANING_RAW_DATA_RANGE}日以内のものを対象とする
		$elapse_day_from = $day_alive;
		$elapse_day_to = $day_alive + PX2_CLEANING_RAW_DATA_RANGE;
		$sql .= "BETWEEN {$elapse_day_from} AND {$elapse_day_to}";
	} else {
		// $day_aliveが指定されていない場合
		// 経過日数が{end_page_switch_date}日以上{$end_page_switch_date + PX2_CLEANING_RAW_DATA_RANGE}日以内のものを対象とする
		$sql .= "BETWEEN pi.end_page_switch_date AND (pi.end_page_switch_date + " . PX2_CLEANING_RAW_DATA_RANGE . ")";
	}
	$sql = preg_replace('/\s+/', ' ', $sql);

	$query = @pg_query($db, $sql);
	if (!$query) {
		putlog($sql . ',ERROR');
		// PostgreSQLのエラー内容をログに出力したい場合はこの行を有効にする
		putlog(pg_last_error($db));
		return FALSE;
	}

	// 検索条件のSQLをログに残したくない時はコメントアウトする
	putlog($sql . ',SUCCESS');

	if (pg_num_rows($query) > 0) {
		$cleaning_target_list = pg_fetch_all($query);
	} else {
		$cleaning_target_list = array();
	}
	
	// DBから切断
	pg_close($db);
	
	return $cleaning_target_list;
}


/*----------------------------------------------------------------------------*
 回答テーブルのお掃除
 $db_connstr		接続先のDB接続文字列 (px2_db20またはpx2_db90になる)
 $cleaning_target_list	お掃除対象のリスト
 *----------------------------------------------------------------------------*/
function cleaning_rs_tables($db_connstr, $cleaning_target_list) {

	$db = pg_connect($db_connstr);

	$success_list = array();
	$error_list = array();

	$ank_drop_table_sql_list = array();

	// お掃除対象のリストを順次処理する
	foreach ($cleaning_target_list as $cleaning_target) {

		// 実行するSQLの配列(案件単位)
		$ank_drop_table_sql_list = array();

		// SQL作成 (テーブル削除)
		$sql = "SELECT tablename FROM pg_tables WHERE schemaname = 'pyxis2_suv' AND " .
		       "tablename ~ '^rs_[a-z]+_" . $cleaning_target['nxs_ank_book_seq'] . "_[0-9]+(_[0-9]+)?$'";
		$query = @pg_query($db, $sql);
		if ($query) {
			while ($row = pg_fetch_object($query)) {
				$ank_drop_table_sql_list[] = "TRUNCATE TABLE {$row->tablename}; DROP TABLE {$row->tablename}";
			}
		} else {
			putlog($sql . ',ERROR');
			$errormsg = pg_last_error($db);
			putlog($errormsg);
			$success = FALSE;
		}

		// トランザクション開始
		$success = TRUE;
		@pg_query($db, "BEGIN");

		// SQLを実行する
		foreach ($ank_drop_table_sql_list as $ank_sql) {
			$result = @pg_query_update($db, $ank_sql);
			if ($result) {
				putlog("{$ank_sql},SUCCESS");
			} else {
				putlog("{$ank_sql},ERROR");
				$errormsg = pg_last_error($db);
				putlog($errormsg);
				$success = FALSE;
				break;
			}
		}

		// トランザクション終了
		if ($success) {
			if (PX2_CLEANING_DRYRUN > 0) {
				// DRYRUN=1,2の時はロールバックする
				@pg_query($db, "ROLLBACK");
				putlog("[DRYRUN] ROLLBACK {$cleaning_target['nxs_ank_book_seq']}");
			} else {
				@pg_query($db, "COMMIT");
			}
			$success_list[] = $cleaning_target;
		} else {
			@pg_query($db, "ROLLBACK");
			$error_list[] = $cleaning_target;
		}
	}

	pg_close($db);


	// 結果の生成
	$result = array();
	$result['success_list'] = $success_list;
	$result['error_list'] = $error_list;

	return $result;

}


/*----------------------------------------------------------------------------*
 実査管理DBのお掃除
 $cleaning_target_list	お掃除対象のリスト
 *----------------------------------------------------------------------------*/
function cleaning_rsa_tables($cleaning_target_list) {

	$success_list = array();
	$error_list = array();


	// お掃除対象のリストを順次処理する
	foreach ($cleaning_target_list as $cleaning_target) {

		// アンケートIDからDB接続文字列を取得して接続
		$db = pg_connect(get_dbconnectstr($cleaning_target['nxs_ank_book_seq']));

		// 実行するSQLの配列(案件単位)
		$ank_drop_table_sqllist = array();
		$ank_drop_sequence_sqllist = array();
		$ank_delete_sqllist = array();


		// SQL作成 (テーブル削除)
		$sql = "SELECT tablename FROM pg_tables WHERE schemaname = 'pyxis2_suv' " .
		       "AND tablename ~ '^(ans_server_info|ent_list)_" . $cleaning_target['nxs_ank_book_seq'] . "$' ";
		$query = @pg_query($db, $sql);
		if ($query) {
			while ($row = pg_fetch_object($query)) {
				$ank_drop_table_sqllist[] = "TRUNCATE TABLE {$row->tablename}; DROP TABLE {$row->tablename}";
			}
		} else {
			putlog($sql . ',ERROR');
			$errormsg = pg_last_error($db);
			putlog($errormsg);
			$success = FALSE;
		}

		// SQL作成 (シーケンス削除)
		$seqname = "pyxis2_suv.rs_attribute_{$cleaning_target['nxs_ank_book_seq']}_seq";
		$ank_drop_sequence_sqllist[] = "DROP SEQUENCE IF EXISTS {$seqname}";

		// SQL作成 (レコード削除)
		$ank_delete_sqllist[] = "DELETE FROM pyxis2_sys.qtp_quota_cntup WHERE nxs_ank_book_seq = {$cleaning_target['nxs_ank_book_seq']}";
		$ank_delete_sqllist[] = "DELETE FROM pyxis2_sys.qtp_quota_cellinfo WHERE nxs_ank_book_seq = {$cleaning_target['nxs_ank_book_seq']}";
		$ank_delete_sqllist[] = "DELETE FROM pyxis2_sys.qtp_quota_table WHERE nxs_ank_book_seq = {$cleaning_target['nxs_ank_book_seq']}";
		$ank_delete_sqllist[] = "DELETE FROM pyxis2_sys.prj_panelinfo WHERE nxs_ank_book_seq = {$cleaning_target['nxs_ank_book_seq']}";
		$ank_delete_sqllist[] = "DELETE FROM pyxis2_sys.eqt_feeinfo WHERE nxs_ank_book_seq = {$cleaning_target['nxs_ank_book_seq']}";
		$ank_delete_sqllist[] = "DELETE FROM pyxis2_sys.eqt_info WHERE nxs_ank_book_seq = {$cleaning_target['nxs_ank_book_seq']}";
		$ank_delete_sqllist[] = "DELETE FROM pyxis2_sys.prj_stop_info WHERE nxs_ank_book_seq = {$cleaning_target['nxs_ank_book_seq']}";


		// トランザクション開始
		$success = TRUE;
		@pg_query($db, "BEGIN");

		// 実行するSQLをひとまとめにする
		$ank_sqllist = array_merge(
			$ank_drop_table_sqllist,
			$ank_drop_sequence_sqllist,
			$ank_delete_sqllist
		);

		// SQLを実行する
		foreach ($ank_sqllist as $ank_sql) {
			$result = @pg_query_update($db, $ank_sql);
			if ($result) {
				putlog("{$ank_sql},SUCCESS");
			} else {
				putlog("{$ank_sql},ERROR");
				$errormsg = pg_last_error($db);
				putlog($errormsg);
				$success = FALSE;
				break;
			}
		}

		// トランザクション終了
		if ($success) {
			if (PX2_CLEANING_DRYRUN > 0) {
				// DRYRUN=1,2の時はロールバックする
				@pg_query($db, "ROLLBACK");
				putlog("[DRYRUN] ROLLBACK {$cleaning_target['nxs_ank_book_seq']}");
			} else {
				@pg_query($db, "COMMIT");
			}
			$success_list[] = $cleaning_target;
		} else {
			@pg_query($db, "ROLLBACK");
			$error_list[] = $cleaning_target;
		}

		// VACUUMを実行
		@pg_query($db, "VACUUM pyxis2_sys.qtp_quota_cntup");
		@pg_query($db, "VACUUM pyxis2_sys.qtp_quota_cellinfo");
		@pg_query($db, "VACUUM pyxis2_sys.qtp_quota_table");
		@pg_query($db, "VACUUM pyxis2_sys.prj_panelinfo");
		@pg_query($db, "VACUUM pyxis2_sys.eqt_feeinfo");
		@pg_query($db, "VACUUM pyxis2_sys.eqt_info");
		@pg_query($db, "VACUUM pyxis2_sys.prj_stop_info");

		pg_close($db);
	}


	$result = array();
	$result['success_list'] = $success_list;
	$result['error_list'] = $error_list;

	return $result;
}


/*----------------------------------------------------------------------------*
 一時ディレクトリやアンケートディレクトリを削除する
 $basedir		ベースディレクトリ
 $cleaning_target_list	お掃除対象のリスト
 $target_key		各ディレクトリ名を示すキー
 $apply_getEqtDir	各ディレクトリにgetEqtDirを適用するか
 $suffix		各ディレクトリの末尾につける文字列 (''または'_pre')
 *----------------------------------------------------------------------------*/
function cleaning_dir($basedir, $cleaning_target_list, $target_key, $apply_getEqtDir=FALSE, $suffix='') {

	$success_list = array();
	$error_list = array();

	foreach ($cleaning_target_list as $cleaning_target) {
		$target = $cleaning_target[$target_key];
		if ($apply_getEqtDir) $target = getEqtDir($target);
		$targetdir = $basedir . '/' . $target . $suffix;
		if (! is_dir($targetdir)) {
			// ディレクトリが存在しない場合にエラーとして扱う場合は以下を有効にする
			// $error_list[] = $cleaning_target;
			// putlog($cleaning_target[$target_key] . ',ERROR (not exists)');
			continue;
		}
		if (! rm_rf($targetdir)) {
			// ディレクトリの削除に失敗
			$error_list[] = $cleaning_target;
			putlog($cleaning_target[$target_key] . ',ERROR');
			continue;
		}
		// 成功した
		$success_list[] = $cleaning_target;
		putlog($cleaning_target[$target_key] . ',SUCCESS');
	}

	// 結果の生成
	$result = array();
	$result['success_list'] = $success_list;
	$result['error_list'] = $error_list;
	
	return $result;

}



/*----------------------------------------------------------------------------*
 お掃除対象のリストから該当するアンケートディレクトリを削除する
 $basedir		アンケートディレクトリが配置されているディレクトリのパス
 $cleaning_target_list	お掃除対象のリスト
 $suffix		アンケートディレクトリの末尾につける文字列 (''または'_pre')
 *----------------------------------------------------------------------------*/
function cleaning_sourcedir($basedir, $cleaning_target_list, $suffix='') {

	return cleaning_dir($basedir, $cleaning_target_list, 'nxs_enquete_no', TRUE, $suffix);

}


/*----------------------------------------------------------------------------*
 お掃除対象のリストから該当する一時データを削除する
 $basedir		一時データが配置されているディレクトリのパス
 $cleaning_target_list	お掃除対象のリスト
 *----------------------------------------------------------------------------*/
function cleaning_tmpdir($basedir, $cleaning_target_list) {

	return cleaning_dir($basedir, $cleaning_target_list, 'nxs_ank_book_seq');

}


/*----------------------------------------------------------------------------*
 開発管理DBのお掃除
 $db_connstr		接続先のDB接続文字列 (px2_db10)
 $cleaning_target_list	お掃除対象のリスト
 *----------------------------------------------------------------------------*/
function cleaning_dva_tables($db_connstr, $cleaning_target_list) {

	$db = pg_connect($db_connstr);

	$success_list = array();
	$error_list = array();

	// お掃除対象のリストを順次処理する
	foreach ($cleaning_target_list as $cleaning_target) {

		// 実行するSQLの配列(案件単位)
		$ank_drop_table_sqllist = array();
		$ank_drop_sequence_sqllist = array();
		$ank_delete_sqllist = array();


		// SQL作成 (テーブル削除)
		// pg_tablesからお掃除対象のテーブルを探す
		$sql = "SELECT tablename FROM pg_tables WHERE schemaname = 'pyxis2_suv' AND (" .
		       "tablename ~ '^rs_[a-z]+_" . $cleaning_target['nxs_ank_book_seq'] . "_[0-9]+(_[0-9]+)?$'" .
		       "OR " .
		       "tablename ~ '^(ans_server_info|ent_list)_" . $cleaning_target['nxs_ank_book_seq'] . "$')";
		$query = @pg_query($db, $sql);
		if ($query) {
			while ($row = pg_fetch_object($query)) {
				$ank_drop_table_sqllist[] = "TRUNCATE TABLE {$row->tablename}; DROP TABLE {$row->tablename}";
			}
		} else {
			putlog($sql . ',ERROR');
			$errormsg = pg_last_error($db);
			putlog($errormsg);
			$success = FALSE;
		}

		// SQL作成 (シーケンス削除)
		$seqname = "pyxis2_suv.rs_attribute_{$cleaning_target['nxs_ank_book_seq']}_seq";
		$ank_drop_sequence_sqllist[] = "DROP SEQUENCE IF EXISTS {$seqname}";

		// SQL作成 (レコード削除)
		$ank_delete_sqllist[] = "DELETE FROM pyxis2_sys.qtp_quota_cntup WHERE nxs_ank_book_seq = {$cleaning_target['nxs_ank_book_seq']}";
		$ank_delete_sqllist[] = "DELETE FROM pyxis2_sys.qtp_quota_cellinfo WHERE nxs_ank_book_seq = {$cleaning_target['nxs_ank_book_seq']}";
		$ank_delete_sqllist[] = "DELETE FROM pyxis2_sys.qtp_quota_table WHERE nxs_ank_book_seq = {$cleaning_target['nxs_ank_book_seq']}";
		$ank_delete_sqllist[] = "DELETE FROM pyxis2_sys.prj_stop_info WHERE nxs_ank_book_seq = {$cleaning_target['nxs_ank_book_seq']}";
		$ank_delete_sqllist[] = "DELETE FROM pyxis2_sys.prj_delivery_info WHERE nxs_ank_book_seq = {$cleaning_target['nxs_ank_book_seq']}";

		// SQL作成 (レコード削除・XMLデータ削除)
		// パート毎に最終更新バージョンIDを得る
		$sql = "SELECT nxs_ank_book_seq, nxs_enquete_no, MAX(ver_id) AS max_ver_id FROM pyxis2_sys.eqtc_src_ver_ctl " .
		       "WHERE nxs_ank_book_seq = {$cleaning_target['nxs_ank_book_seq']} " .
		       "GROUP BY nxs_ank_book_seq, nxs_enquete_no";
		$query = @pg_query($db, $sql);
		
		if ($query) {
			while ($row = pg_fetch_object($query)) {
				// 最終更新バージョン以下のアンケートXMLの内容をクリアする
				$ank_delete_sqllist[] = "UPDATE pyxis2_sys.eqtc_src_ver_ctl SET questionnaire_xml = NULL " .
							"WHERE nxs_ank_book_seq = {$row->nxs_ank_book_seq} " .
							"AND nxs_enquete_no = {$row->nxs_enquete_no} " .
							"AND ver_id < '{$row->max_ver_id}'";
			}
		} else {
			putlog($sql . ',ERROR');
			$errormsg = pg_last_error($db);
			putlog($errormsg);
			$success = FALSE;
		}


		// トランザクション開始
		$success = TRUE;
		@pg_query($db, "BEGIN");

		// 実行するSQLをひとまとめにする
		$ank_sqllist = array_merge(
			$ank_drop_table_sqllist,
			$ank_drop_sequence_sqllist,
			$ank_delete_sqllist
		);

		// SQLを実行する
		foreach ($ank_sqllist as $ank_sql) {
			$result = @pg_query_update($db, $ank_sql);
			if ($result) {
				// putlog("{$ank_sql},SUCCESS");
			} else {
				// putlog("{$ank_sql},ERROR");
				$success = FALSE;
				break;
			}
		}

		// トランザクション終了
		if ($success) {
			if (PX2_CLEANING_DRYRUN > 0) {
				// DRYRUN=1,2の時はロールバックする
				@pg_query($db, "ROLLBACK");
				putlog("[DRYRUN] ROLLBACK {$cleaning_target['nxs_ank_book_seq']}");
			} else {
				@pg_query($db, "COMMIT");
			}
			$success_list[] = $cleaning_target;
			putlog($cleaning_target['nxs_ank_book_seq'] . ',SUCCESS');
		} else {
			@pg_query($db, "ROLLBACK");
			$error_list[] = $cleaning_target;
			putlog($cleaning_target['nxs_ank_book_seq'] . ',ERROR');
		}
	}

	// VACUUMを実行
	@pg_query($db, "VACUUM pyxis2_sys.qtp_quota_cntup");
	@pg_query($db, "VACUUM pyxis2_sys.qtp_quota_cellinfo");
	@pg_query($db, "VACUUM pyxis2_sys.qtp_quota_table");
	@pg_query($db, "VACUUM pyxis2_sys.eqtc_src_ver_ctl");
	@pg_query($db, "VACUUM pyxis2_sys.prj_stop_info");
	@pg_query($db, "VACUUM pyxis2_sys.prj_delivery_info");

	pg_close($db);

	$result = array();
	$result['success_list'] = $success_list;
	$result['error_list'] = $error_list;
	
	return $result;

}


/*----------------------------------------------------------------------------*
 お掃除結果のリストからレポートメールを送信する
 $paramsに必要なパラメーターを連想配列で渡す。
 
 from		送信元のメールアドレス
 to		送信先のメールアドレス
 subject	件名

 *** 以下は本文中で使用するパラメーター *** 
 server		件名および本文に記載するサーバー名
 title		本文中に記載するタイトル
 script		本文中に記載するスクリプトファイル名
 log		本文中に記載するログファイル名
 target_error	お掃除対象に失敗した場合はTRUE
 data_success	データの削除に成功したアンケート数 (任意)
 data_error	データの削除に失敗したアンケート数 (任意)
 src_success	ソースの削除に成功したアンケート数 (任意)
 src_error	ソースの削除に失敗したアンケート数 (任意)
 start		処理開始日時
 finish		処理完了日時
 date		処理日時
 *----------------------------------------------------------------------------*/
function send_reportmail($params) {

	extract($params);

	$date = date('Y/m/d H:i:s', $date);
	$start = date('Y/m/d(D) H:i:s', $start);
	$finish = date('Y/m/d(D) H:i:s', $finish);

	// 本文の作成
	$body = array();
	$body[] = "Server : {$server}";
	$body[] = "Title : {$title}";
	$body[] = "Date : {$date}";
	$body[] = "Script : {$script}";
	$body[] = "Log : {$log}";
	$body[] = "";
	$body[] = "[RESULT]";

	if ($target_error) {
		// お掃除対象の取得エラー発生時はこちら
		$body[] = "Failed to get targetlist.";
	} else {
		// データ削除結果
		if (isset($data_success) and isset($data_error)) {
			$body[] = "Drop Enquete Data Success : {$data_success}";
			$body[] = "Drop Enquete Data Error : {$data_error}";
		}

		// ソース削除結果
		if (isset($src_success) and isset($src_error)) {
			$body[] = "Drop Enquete Source Success : {$src_success}";
			$body[] = "Drop Enquete Source Error : {$src_error}";
		}

		// 開始終了
		$body[] = "Drop start : {$start}";
		$body[] = "Drop finish : {$finish}";
	}

	$body = implode(PHP_EOL, $body);
	$subject = sprintf($subject, $server, $date);
	$headers = array();
	$headers[] = "From: {$from}";
	$headers[] = "Reply-To: {$from}";
	$headers = implode(PHP_EOL, $headers);

	// sendmailコマンドで送信するのでFromヘッダーはadditional_parametersを使用する
	// http://php.net/manual/ja/function.mail.php
	// 例3 追加ヘッダ及び追加コマンドラインパラメータを指定してメールを送信する
	$params = "-f " . $from;

	// メールの送信
	if (! @mb_send_mail($to, $subject, $body, $headers, $params)) {
		putlog("Faild to send report mail. From:{$from} To:{$to}");
	}

}


/*----------------------------------------------------------------------------*
 PX2_CLEANING_LOGNAME定数で指定されたファイルにログを書き出す
 $message	ログメッセージ
 ※行頭にはタイムスタンプを挿入する。
 *----------------------------------------------------------------------------*/
function putlog($message) {
	$message = date('Y-m-d H:i:s') . ',' . $message . PHP_EOL;
	file_put_contents(PX2_CLEANING_LOGNAME, $message, FILE_APPEND);
}


/*----------------------------------------------------------------------------*
 暗号化されたアンケートディレクトリ名を返す
 $partNo	パートNo
 *----------------------------------------------------------------------------*/
function getEqtDir($partNo) {
	if (PX2_CLEANING_PART_ENCRYPT) {
		$data = mcrypt_ecb(MCRYPT_3DES, "cAe189caDB2" , $partNo, MCRYPT_ENCRYPT, "\0\0\0\0\0\0\0\0");
		$data = bin2hex($data);
		return $data;
	} else {
		return $partNo;
	}
}


/*----------------------------------------------------------------------------*
 アンケートIDから実査管理DBサーバーを特定してDB接続文字列を返す
 $nxs_ank_book_seq	アンケートID
 *----------------------------------------------------------------------------*/
function get_dbconnectstr($nxs_ank_book_seq) {
        $connstr = NULL;

        $db = pg_connect(DEVELOP_DB_CONNECT_STRING);

        $sql = "select connect_target from pyxis2_sys.prj_connect_server " .
               "where nxs_ank_book_seq = {$nxs_ank_book_seq} and server_type = 'px2_db50'";
        $query = @pg_query($db, $sql);

        if ($query and pg_num_rows($query) == 1) {
                $row = pg_fetch_object($query);
                $serverID = (int)str_replace('px2_db50_', '', $row->connect_target);
                $connstr = constant('REAL_ADMIN_DB_CONNECT_STRING_' . $serverID );
        }

        pg_close($db);

        return $connstr;
}


/*----------------------------------------------------------------------------*
 指定されたディレクトリを再帰的に削除する
 $dir		削除対象のディレクトリ
 ※PX2_CLEANING_DRYRUN>0の場合には実際には削除しないで削除されるはずの
  ディレクトリ名を標準出力に出力するだけにする。
 *----------------------------------------------------------------------------*/
function rm_rf($dir) {
	if (PX2_CLEANING_DRYRUN > 0) {
		echo "[DRYRUN] rm_rf: " . $dir . PHP_EOL;
		return TRUE;
	}

	if (!is_dir($dir)) {
		return FALSE;
	} else {
		$filelist = scandir($dir);
		foreach ($filelist as $filename) {
			if ($filename == '.' || $filename == '..') {
				continue;
			}
			if (is_dir("{$dir}/{$filename}")) {
				rm_rf("{$dir}/{$filename}");
			} else {
				unlink("{$dir}/{$filename}");
			}
		}
	}
	rmdir($dir);
	return TRUE;
}


/*----------------------------------------------------------------------------*
 更新系SQLを実行する
 ※PX2_CLEANING_DRYRUN=2の場合には実際には実行しないで実行されるはずの
  SQLを標準出力に出力するだけにする。
 *----------------------------------------------------------------------------*/
function pg_query_update($conn, $sql) {
	if (PX2_CLEANING_DRYRUN == 2) {
		echo "[DRYRUN] pg_query_update: " . $sql . PHP_EOL;
		return TRUE;
	} else {
		return @pg_query($conn, $sql);
	}
}


