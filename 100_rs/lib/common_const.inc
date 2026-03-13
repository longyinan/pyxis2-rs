<?php

/******************************************************************************
 common_const.inc

 以下の定数を定義する。
 PX2_PRODUCTION_ENV  本番環境の場合はTRUE, 検証環境の場合はFALSE

 以下の定数を環境変数から自動的に定義する。
 SEVER_ID            自身の実査サーバーID
 SEVER_HOSTNAME      自身の実査サーバーホスト名
 IS_PX2_RS           実査サーバーの場合はTRUE, そうでない場合はFALSE
 ******************************************************************************/


// Pyxis2本番環境の場合はTRUE、Pyxis2検証環境の場合はFALSEを指定する。
define('PX2_PRODUCTION_ENV', TRUE);


$server_id = NULL;
$server_hostname = NULL;
$is_px2_rs = FALSE;

if (PX2_PRODUCTION_ENV) {

	/*-----------------------------------------------------------------------------
	 Pyxis2本番環境の場合

	 実査サーバーは複数存在するため、サーバーIDを含めたホスト名を設定することで
	 ホスト名から自動的にサーバーIDを決定できるようにする。
	 -----------------------------------------------------------------------------*/

	// 実査サーバーのホスト名パターン
	// サーバーIDに相当する数値部分を取得できるようにする
	define('PX2_RS_PATTERN', '/^vmp2-local-rs([0-9]+)$/i');

	// 実査サーバーの場合は環境変数からホスト名を取得して定数化
	// ホスト名からサーバーIDを決定して定数化
	$hostname = php_uname('n');
	if (preg_match(PX2_RS_PATTERN, $hostname, $matches)) {
		// 実査サーバーの場合
		$server_id = (int)$matches[1];
		$server_hostname = $hostname;
		$is_px2_rs = TRUE;
	}
	else{
		// アンケート開発環境の場合
		$server_id = 1;
	}

} else {

	/*-----------------------------------------------------------------------------
	 Pyxis2検証環境の場合

	 残念ながら検証環境は寄せ集めでホスト名に規則がないため個別にサーバーIDを指定。
	 -----------------------------------------------------------------------------*/

	// 実査サーバーの場合は環境変数からホスト名を取得して定数化
	// ホスト名からサーバーIDを決定して定数化
	$hostname = php_uname('n');
	if ($hostname === FALSE) {
		// ローカルの開発環境の場合
		$server_id = 1;
		$server_hostname = $_SERVER['HTTP_HOST'];
	} else {
		// 検証環境の場合
		// 検証環境では命名規則がないので連想配列$mapを対応表とする
		$map = array(
			'csps-vfy05db' => array('server_id' => 1, 'server_hostname' => 'px2_admin'),
			'csps-vfy03db' => array('server_id' => 1, 'server_hostname' => 'px2_rs01'),
			//'csps-vfy04db' => array('server_id' => 2, 'server_hostname' => 'px2_rs02'),
			//'csps-vfy01pl' => array('server_id' => 3, 'server_hostname' => 'px2_rs03'),
		);

		if (array_key_exists($hostname, $map)) {
			$server_id = $map[$hostname]['server_id'];
			$server_hostname = $map[$hostname]['server_hostname'];
			$is_px2_rs = TRUE;
		}
	}

}


// define('SEVER_ID', $server_id);
// define('SEVER_HOSTNAME', $server_hostname);
// define('IS_PX2_RS', $is_px2_rs);
define('SEVER_ID', 1);
define('SEVER_HOSTNAME', $_SERVER['HTTP_HOST']);
define('IS_PX2_RS', true);