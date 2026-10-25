【島輝】輸入法 DaoHui
=====================

島輝是 [【小狼毫】Weasel](https://github.com/rime/weasel) 的改名獨立分支，
由 RIME 團隊的小狼毫 0.17.4 修改而來，基於 中州韻輸入法引擎／Rime Input Method Engine 等開源技術。

DaoHui is a renamed, independent fork of Weasel (小狼毫), the Windows frontend of
the Rime Input Method Engine, by the RIME team. It installs side by side with stock
Weasel: it has its own text-service CLSID, file names, registry key, IPC names and
user folder (see [DAOHUI.md](DAOHUI.md)).

原作：式恕堂／RIME Developers（小狼毫）　分支：島輝貢獻者

授權條款：GPLv3（見 LICENSE.txt）

項目主頁：https://github.com/TimLin-1231/rime-asus-zhuyin

上游項目：https://github.com/rime/weasel ／ https://rime.im

與小狼毫的差異
--------------

  * 焦點離開、或應用程式結束組字時，保留畫面上還沒送出的字（keep-composition 修補）。
  * 改名為島輝，可與小狼毫同時安裝：不同的 TSF CLSID、檔名、註冊表位置
    （`Software\DaoHui`）、IPC 名稱，使用者資料夾為 `%AppData%\DaoHui`。
  * 更新頻道指向島輝自己的 appcast。

安裝輸入法
----------

本品適用於 64 位元 Windows 8.1 ~ Windows 11

初次安裝時，安裝程序將顯示「安裝選項」對話框。

若要將【島輝】註冊到繁體中文（臺灣）鍵盤佈局，請在「輸入語言」欄選擇「中文（臺灣）」，再點擊「安裝」按鈕。

安裝完成後，仍可由開始菜單打開「安裝選項」更改輸入語言。

使用輸入法
----------

選取輸入法指示器菜單裏的【中】字樣圖標，開始用島輝寫字。

可通過快捷鍵 <kbd>Ctrl+`</kbd> 或 <kbd>F4</kbd> 呼出方案選單、切換輸入方式。

定製輸入法
----------

通過 開始菜單 » 島輝輸入法 訪問設定工具及常用位置。

用戶詞庫、配置文件位於 `%AppData%\DaoHui`，可通過菜單中的「用戶文件夾」打開。
島輝沿用小狼毫的前端設定檔名稱 `weasel.yaml`／`weasel.custom.yaml`，小狼毫的用戶資料可以直接複製過來使用。

修改詞庫、配置文件後，須「重新部署」方可生效。

定製 Rime 的方法，請參考 Wiki [《定製指南》](https://github.com/rime/home/wiki/CustomizationGuide)。前端樣式和行為與小狼毫相同，可參考小狼毫的 [Wiki 頁面](https://github.com/rime/weasel/wiki)。

從原始碼編譯：見 [DAOHUI.md](DAOHUI.md)。

致謝
----

島輝的程式幾乎全部來自小狼毫。以下為小狼毫原有的致謝。

### 輸入方案設計：

  * 【朙月拼音】系列及【八股文】詞典
    - 部分數據來源於 CC-CEDICT、Android 拼音、新酷音、opencc 等開源項目
    - 維護者：佛振、瑾昀
  * 【注音／地球拼音】
    - 維護者：佛振、瑾昀
  * 【倉頡五代】
    - 發明人：朱邦復先生
    - 碼表源自 www.chinesecj.com
    - 構詞碼表作者：惜緣

  【五笔】【粵拼】【上海／蘇州吳語】【中古漢語拼音】【國際音標】等衆多方案
  不再以安裝包預裝形式提供。可由 <https://github.com/rime/plum> 下載安裝。

### 程序設計（小狼毫）：

  * [佛振](https://github.com/lotem)
  * [鄒旭](https://github.com/zouxu09)
  * [Xiangyan Sun](https://github.com/wishstudio)
  * [Prcuvu](https://github.com/Prcuvu)
  * [nameoverflow](https://github.com/nameoverflow)
  * [fxliang](https://github.com/fxliang)
  * [Azuk 443](https://github.com/determ1ne)

  查看更多 [代碼貢獻者](https://github.com/rime/weasel/graphs/contributors)

### 美術：

  * 圖標設計／[Patricivs](https://github.com/Patricivs)
  * 配色方案／Aben、P1461、Patricivs、skoj、佛振、五磅兔

### 本品引用了以下開源軟件：

  * [Weasel／小狼毫](https://github.com/rime/weasel) (GNU General Public License v3.0)
  * [Boost C++ Libraries](http://www.boost.org/) (Boost Software License)
  * [curl](https://curl.haxx.se/) (MIT/X derivate license)
  * [google-glog](https://github.com/google/glog) (BSD 3-Clause License)
  * [Google Test](https://github.com/google/googletest) (BSD 3-Clause License)
  * [LevelDB](https://github.com/google/leveldb) (BSD 3-Clause License)
  * [librime](https://github.com/rime/librime) (BSD 3-Clause License)
  * [marisa-trie](https://github.com/s-yata/marisa-trie) (BSD 2-Clause License, LGPL 2.1)
  * [OpenCC / 開放中文轉換](https://github.com/BYVoid/OpenCC) (Apache License 2.0)
  * [plum](https://github.com/rime/plum) (GNU Lesser General Public License v3.0)
  * [WinSparkle](https://github.com/vslavik/winsparkle) (MIT License)
  * [yaml-cpp](https://github.com/jbeder/yaml-cpp) (MIT License)
  * [7-Zip](https://www.7-zip.org) (GNU LGPLv2.1+ with unRAR restriction)

問題與反饋
----------

島輝的問題請到 <https://github.com/TimLin-1231/rime-asus-zhuyin/issues> 反饋，
請不要把島輝特有的問題回報到小狼毫。

Rime 輸入法（不限於 Windows 平臺）功能、使用方法與配置相關的問題，請參考
<https://github.com/rime/home>

謝謝 RIME 團隊與小狼毫的所有貢獻者！
