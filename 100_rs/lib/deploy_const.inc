<?php

/******************************************************************************
 deploy_const.inc

 デプロイ機能で使用する定数ファイル。
 ******************************************************************************/


/*----------------------------------------------------------------------------*
 ホスト名およびサーバーIDの定数を取得
 *----------------------------------------------------------------------------*/
require_once(dirname(__FILE__) . '/common_const.inc');


/*----------------------------------------------------------------------------*
 開発管理DBへの接続情報 (デプロイ指示を確認、更新するために使用)
 *----------------------------------------------------------------------------*/
define('DEVADMIN_DB_CONNECT_STRING', 'host=10.73.240.3 port=5432 dbname=px2_db10 user=px2_suv_user password=%)la+61fOc{L+GlI');


/*----------------------------------------------------------------------------*
 自サーバーの実査回答DBへの接続情報 (回答テーブルの作成)
 *----------------------------------------------------------------------------*/
define('RS_DB_CONNECT_STRING', 'host=10.73.240.3 port=5432 dbname=px2_db90 user=px2_suv_user password=%)la+61fOc{L+GlI');


/*----------------------------------------------------------------------------*
 デプロイ関連: ステータスコード等
 *----------------------------------------------------------------------------*/
// インストール状況: 処理待ち
define('PX2_INSTALL_STATE_WAIT', 0);
// インストール状況: 処理中
define('PX2_INSTALL_STATE_PROC', 1);
// インストール状況: 完了
define('PX2_INSTALL_STATE_DONE', 2);
// インストール状況: エラー
define('PX2_INSTALL_STATE_ERROR', 3);


/*----------------------------------------------------------------------------*
 デプロイ関連: その他
 *----------------------------------------------------------------------------*/
// アーカイブダウンロード用URL
define('PX2_DEPLOYARCHIVE_BASEURL', 'http://vmp2-local-dva01/deploy');
// アーカイブ保存先
define('PX2_DEPLOYARCHIVE_SAVEDIR', dirname(__FILE__) . '/../deploy_data');
// アンケートディレクトリ配置先
define('PX2_PRJ_BASEDIR', dirname(__FILE__) . '/../htdocs');
// 共通ファイル配置先
define('PX2_CMN_BASEDIR', dirname(__FILE__) . '/../htdocs');
// 共通ファイルディレクトリをロールバック用に退避する際に末尾に付加する文字列
define('PX2_CMN_DIR_SUFFIX', '_bak');
// インストールスクリプトのディレクトリ
define('PX2_INSTALL_SCRIPTDIR', 'install');
// インストールスクリプトファイル名
define('PX2_INSTALL_SCRIPT', 'install.php');
// ロールバックスクリプトファイル名
define('PX2_ROLLBACK_SCRIPT', 'rollback.php');
// ロールバックスクリプト処理種別 backup
define('PX2_ROLLBACK_PROC_BACKUP', 'backup');
// ロールバックスクリプト処理種別 rollback
define('PX2_ROLLBACK_PROC_ROLLBACK', 'rollback');
// インストールスクリプトで実行するSQLファイル名
define('PX2_INSTALL_INITSQL', 'create_sql.sql');
// インストール実行URL
define('PX2_INSTALL_BASEURL', 'http://' . SEVER_HOSTNAME);
// ダウンロード処理: 接続タイムアウト
define('PX2_DEPLOYARCHIVE_CONNECTTIMEOUT', 5);
// ダウンロード処理: 応答タイムアウト
define('PX2_DEPLOYARCHIVE_TIMEOUT', 60);
// ダウンロード処理: リトライ回数
define('PX2_DEPLOYARCHIVE_RETRY', 3);
// インストール処理: 接続タイムアウト
define('PX2_INSTALL_CONNECTTIMEOUT', 5);
// インストール処理: 応答タイムアウト
define('PX2_INSTALL_TIMEOUT', 60);
