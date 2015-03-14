# [CaboCha/南瓜](http://code.google.com/p/cabocha/): Yet Another Japanese Dependency Structure Analyzer #

## [CaboCha/南瓜](http://code.google.com/p/cabocha/)とは ##
[CaboCha](http://code.google.com/p/cabocha/) は, Support Vector Machines に基づく日本語係り受け解析器です。

## 特徴 ##
  * Support Vector Machines (SVMs) に基づく, 高性能な係り受け解析器
  * SVM の分類アルゴリズムの高速化手法である PKE (ACL 2003 にて発表)を適用.
  * IREX の定義による固有表現解析が可能
  * 柔軟な入力形式. 生文はもちろん, 形態素解析済みデータ, 文節区切り済み データ, 部分的に係り関係が付与されたデータからの解析が可能
  * 係り受けの同定に使用する素性をユーザ側で再定義可能
  * データを用意すれば, ユーザ側で学習を行うことが可能
  * 内部の辞書に, 高速な Trie 構造である Double-Array を採用
  * C/C++/Perl/Ruby ライブラリの提供

## 更新履歴 ##
  * 2015/01/24 0.69
    * Chunk::(head\_pos|func\_pos|token\_size|tokens\_pos) を size\_t に変更
    * FreeListのバッファオーバーフロー問題の修正

  * 2014/03/10 0.68
    * トーナメントモデルの削除
    * -u オプションの追加 (MeCabの -u と同一オプション)

  * 2013/12/31 0.67
    * 解析速度を20%程度高速化
    * OSX上でのビルドエラーの修正
    * Windows版でただしくバージョン番号が表示されないバグの修正

## ダウンロード ##
[こちら](https://drive.google.com/folderview?id=0B4y35FiV1wh7cGRCUUJHVTNJRnM&usp=sharing#list) から最新版をダウンロードします。

  * [CaboCha](http://code.google.com/p/cabocha/) はフリーソフトウェアです。LGPL(Lesser GNU General Public License), または new BSD ライセンスに従って本ソフトウェアを使用,再配布することができます。
  * 付属のモデルファイル(model 以下のファイル)にはBSDライセンス は適用されません。BSDライセンス は, 付属のプログラムや学習ツールのソース・コード, ドキュメントのみに適用されます。
  * 付属のモデルファイルは, 株式会社毎日新聞社の御厚意により 配布されている毎日新聞記事CD-ROM版を基に学習, 作成されたものです。付属のモデルファイルは, 毎日新聞データ使用許諾に関する覚書に記載の利用条件内で その使用及び, 複製，改変，頒布が認められます. そのため配布しているモデルをそのままの形で使うことは, 研究目的, 個人利用に限られます. 誤解ないように言っておくと, これは, [CaboCha](http://code.google.com/p/cabocha/) の利用が研究目的に限定されていることを意味しません。 ご自身で用意なさったコーパスを基に, 独自にモデルを学習, 作成した場合は研究目的以外の利用が可能です。
  * 奈良先端科学技術大学院大学ならびに工藤 拓は, [CaboCha](http://code.google.com/p/cabocha/) のプログラム 及び付属のモデルファイルの保証, および特定目的の適合性の保証を含め いかなる保証をも行うものではなく， [CaboCha](http://code.google.com/p/cabocha/) のプログラム 及び付属のモデルを使用，複製，改変，頒布により生じた損害や賠償等については， 法律上の根拠を問わず一切責任を負いません. ご注意ください。
  * 将来的には, 著作権やライセンス上の問題から, モデルファイルとプログラムを分割 して配布する予定です。
  * [CaboCha](http://code.google.com/p/cabocha/) を参照する場合は, 以下の文献を参照していただけると幸いです。
```
@article{cabocha,
   title = {チャンキングの段階適用による日本語係り受け解析},
   author = {工藤 拓、松本 裕治},
   jorunal = {情報処理学会論文誌},
   volume = 43,
   number = 6,
   pages = {1834-1842},
   year = {2002}
}

@inproceedings{cabocha,
   title = {Japanese Dependency Analysis using Cascaded Chunking},
   author = {Taku Kudo, Yuji Matsumoto},
   booktitle = {CoNLL 2002: Proceedings of the 6th Conference on Natural Language Learning 2002 (COLING 2002 Post-Conference Workshops)},
   pages = {63-69},
   year = {2002}
}
```

## インストール ##
#### UNIX ####
あらかじめインストールすべきプログラム
  * [CRF++](http://crfpp.sourceforge.net) (0.55以降)
  * [MeCab](http://mecab.sourceforge.net) (0.993以降)
  * mecab-ipadic, mecab-jumandic, unidic のいずれか

注意: [ChaSen](http://chasen-legacy.sourceforge.jp/) では動作いたしません。

> インストール手順
```
% ./configure 
% make
% make check
% su
# make install
```

--with-posset オプションにてデフォルトの品詞体系及びモデルを変更することができます。
```
% ./configure --with-posset=juman
```

```
% ./configure --with-posset=unidic
```

#### Windows ####
[MeCab](http://mecab.sourceforge.net)のWindows版を事前にインストールしておく必要があります。
自己解凍インストーラ cabocha-X.XX.exe を実行します。インストールウイザードに従ってインストールしてください。Windows版は IPA品詞体系のモデルのみが含まれています。

#### cabocharcの設定 ####
[CaboCha](http://code.google.com/p/cabocha/)をインストールした後でも、/usr/local/etc/cabocharc を編集することで、異なる品詞体系のモデルを用いることができます。

```
....省略
# posset = IPA
# posset = JUMAN
# posset = UNIDIC

...
# Parser model file name
parser-model  = /usr/local/lib/cabocha/model/dep.ipa.model

# Chunker model file name
chunker-model = /usr/local/lib/cabocha/model/chunk.ipa.model

# NE model file name
ne-model = /usr/local/lib/cabocha/model/ne.ipa.model
```

#### Modelの解説 ####
JUMANモデルは京都大学テキストコーパス 4.0 から直接学習したモデルです。IPA・Unidicモデルは京都大学テキストコーパス4.0をMeCabを使い形態素解析部分だけ自動解析した結果から学習しています。そのため、IPA・Unidicは解析精度がJUMANのものと比べると落ちます。また、Unidic品詞体系は、短単位であり係り受け解析に必ずしも有効な単位・品詞情報が付与されていないため、JUMANモデルに比べ、1%程度解析精度が落ちることが確認されています。

## 使い方 ##
### とりあえず動かしてみる ###
cabocha を起動して, 生文を標準入力から入力してみてください。デフォルトで, 簡易 Tree 表示により結果を出力します。

```
% cabocha
太郎は花子が読んでいる本を次郎に渡した
    太郎は---------D
      花子が-D     |
    読んでいる-D   |
            本を---D
            次郎に-D
              渡した
EOS

```

-f1 というオプションで, 計算機に処理しやすいフォーマットで出力します
```
% cabocha -f1
太郎は花子が読んでいる本を次郎に渡した
* 0 5D 0/1 1.062087
太郎    名詞,固有名詞,人名,名,*,*,太郎,タロウ,タロー
は      助詞,係助詞,*,*,*,*,は,ハ,ワ
* 1 2D 0/1 1.821210
花子    名詞,固有名詞,人名,名,*,*,花子,ハナコ,ハナコ
が      助詞,格助詞,一般,*,*,*,が,ガ,ガ
* 2 3D 0/2 0.000000
読ん    動詞,自立,*,*,五段・マ行,連用タ接続,読む,ヨン,ヨン
で      助詞,接続助詞,*,*,*,*,で,デ,デ
いる    動詞,非自立,*,*,一段,基本形,いる,イル,イル
* 3 5D 0/1 0.000000
本      名詞,一般,*,*,*,*,本,ホン,ホン
を      助詞,格助詞,一般,*,*,*,を,ヲ,ヲ
* 4 5D 1/2 0.000000
次      名詞,一般,*,*,*,*,次,ツギ,ツギ
郎      名詞,一般,*,*,*,*,郎,ロウ,ロー
に      助詞,格助詞,一般,*,*,*,に,ニ,ニ
* 5 -1D 0/1 0.000000
渡し    動詞,自立,*,*,五段・サ行,連用形,渡す,ワタシ,ワタシ
た      助動詞,*,*,*,特殊・タ,基本形,た,タ,タ
EOS
```

### 文字コード変更 ###
特に指定しない限り, euc が使用されます. もし, shift-jis や utf8 を 使いたい場合は, 辞書の configure オプションにて charset を変更しモデルを再構築してください. これで, shift-jis や, utf8 の辞書が作成されます.
```
% ./configure --with-charset=utf8
```

#### UTF-8 only mode ####
configure option で --enable-utf8-only を指定すると. [CaboCha](http://code.google.com/p/cabocha/) が扱う 文字コードを utf8 に固定します. euc-jp や shift-jis をサポートする場合, [CaboCha](http://code.google.com/p/cabocha/) 内部に変換用のテーブルを埋めこみます. --enable-utf8-only を 指定することでテーブルの埋めこみを抑制し, 結果として実行バイナリを 小さくすることができます。

```
% ./configure --with-charset=utf8 --enable-utf8-only
```

### 解析レイヤ ###
[CaboCha](http://code.google.com/p/cabocha/) には, 解析レイヤという概念があります. 解析レイヤは, 以下に示 す 4つの解析単位の事を指します. [CaboCha](http://code.google.com/p/cabocha/) は, これらのレイヤを UNIX pipe のように取り扱います. つまり, 個々の解析レイヤは完全に独立した空間で動作し, ユーザは, 任意のレイヤを入力として与え, 任意のレイヤの出力を得ることができます。さらに, これらのレイヤとは独立に, IREX定義に基づく 固有表現解析機能があります。

解析レイヤは, -I, -O オプションにて変更します。-I は 入力のレイヤ, -O は出力のレイヤです。

```
  -I, --input-layer=LAYER   set input layer
             0 - raw sentence layer
             1 - POS tagged layer
             2 - POS tagged and Chunked layer
             3 - POS tagged, Chunked and Feature selected layer

  -O, --output-layer=LAYER  set ouput layer
             1 - POS tagged layer
             2 - Chunked layer
             3 - Chunked and Feature selected layer
             4 - Parsed layer (default)
```

  * 形態素解析のみ
```
% cabocha -I0 -O1
```
  * 文節切りのみ
```
% cabocha -I0 -O2 
```
  * 係り受け解析まで行う
```
% cabocha -I0 -O4
```

  * cabocha 自身を組みあわせる
```
% mecab | cabocha -I1 -O2 | cabocha -I2 -O3 | cabocha -I3 -O4
```
  * 素性選択は 自分で作成した perl プログラム
```
% cabocha -I0 -O2 | perl myscript.pl | cabocha -I3 -O4
```

### フォーマットの解説 ###
#### 文節区切りレイヤの出力フォーマット ####
形態素解析済みデータに対し, 文節の区切り情報が付与されます. 具体的には, アスタリスクで始まる文節の開始位置を意味する行が追加されます。アスタリスクの後には, 文節番号 (0から始まる整数),係り先番号 (係り先は同定されていないので常に -1) が続きます。

```
太郎は花子が読んでいる本を次郎に渡した
* 0 -1D
太郎    名詞,固有名詞,人名,名,*,*,太郎,タロウ,タロー
は      助詞,係助詞,*,*,*,*,は,ハ,ワ
* 1 -1D
花子    名詞,固有名詞,人名,名,*,*,花子,ハナコ,ハナコ
が      助詞,格助詞,一般,*,*,*,が,ガ,ガ
* 2 -1D
読ん    動詞,自立,*,*,五段・マ行,連用タ接続,読む,ヨン,ヨン
で      助詞,接続助詞,*,*,*,*,で,デ,デ
いる    動詞,非自立,*,*,一段,基本形,いる,イル,イル
* 3 -1D
本      名詞,一般,*,*,*,*,本,ホン,ホン
を      助詞,格助詞,一般,*,*,*,を,ヲ,ヲ
* 4 -1D
次      名詞,一般,*,*,*,*,次,ツギ,ツギ
郎      名詞,一般,*,*,*,*,郎,ロウ,ロー
に      助詞,格助詞,一般,*,*,*,に,ニ,ニ
* 5 -1D
渡し    動詞,自立,*,*,五段・サ行,連用形,渡す,ワタシ,ワタシ
た      助動詞,*,*,*,特殊・タ,基本形,た,タ,タ
EOS
```

#### 素性選択レイヤの出力フォーマット ####
文節切りのフォーマットの他に, 主辞/機能語の位置と任意の個数の素性列が付与されます。例の最初の文節には, "0/1" という情報が 付与されてますが、これは主辞が 0 番目の形態素 (= 太郎)、機能語が 一番目の形態素 (= は) という意味になります。主辞/機能語フィールドは, 素性選択レイヤのコメントに相当し, CaboChaは, このフィルードを解析に使用していません。このフィールドは今後廃止されるかもしれません。

```
太郎は花子が読んでいる本を次郎に渡した
* 0 -1D 0/1 0.000000 F_H0:太郎,F_H1:名詞,F_H2:固有名詞,F_H3:人名,F_H4:名,F_F0:は,F_F1:助詞,F_F2:係助詞,A:は,B:名詞-固有名詞-人名-名,G_CASE:は,F_BOS:1
太郎    名詞,固有名詞,人名,名,*,*,太郎,タロウ,タロー
は      助詞,係助詞,*,*,*,*,は,ハ,ワ
* 1 -1D 0/1 0.000000 F_H0:花子,F_H1:名詞,F_H2:固有名詞,F_H3:人名,F_H4:名,F_F0:が,F_F1:助詞,F_F2:格助詞,F_F3:一般,A:が,B:名詞-固有名詞-人名-名,G_CASE:が
花子    名詞,固有名詞,人名,名,*,*,花子,ハナコ,ハナコ
が      助詞,格助詞,一般,*,*,*,が,ガ,ガ
* 2 -1D 0/2 0.000000 F_H0:読ん,F_H1:動詞,F_H2:自立,F_H5:五段・マ行,F_H6:連用タ接続,F_F0:いる,F_F1:動詞,F_F2:非自立,F_F5:一段,F_F6:基本形,A:基本形,B:動詞-自立
読ん    動詞,自立,*,*,五段・マ行,連用タ接続,読む,ヨン,ヨン
で      助詞,接続助詞,*,*,*,*,で,デ,デ
いる    動詞,非自立,*,*,一段,基本形,いる,イル,イル
* 3 -1D 0/1 0.000000 F_H0:本,F_H1:名詞,F_H2:一般,F_F0:を,F_F1:助詞,F_F2:格助詞,F_F3:一般,A:を,B:名詞-一般,G_CASE:を
本      名詞,一般,*,*,*,*,本,ホン,ホン
を      助詞,格助詞,一般,*,*,*,を,ヲ,ヲ
* 4 -1D 1/2 0.000000 F_H0:郎,F_H1:名詞,F_H2:一般,F_F0:に,F_F1:助詞,F_F2:格助詞,F_F3:一般,A:に,B:名詞-一般,G_CASE:に
次      名詞,一般,*,*,*,*,次,ツギ,ツギ
郎      名詞,一般,*,*,*,*,郎,ロウ,ロー
に      助詞,格助詞,一般,*,*,*,に,ニ,ニ
* 5 -1D 0/1 0.000000 F_H0:渡し,F_H1:動詞,F_H2:自立,F_H5:五段・サ行,F_H6:連用形,F_F0:た,F_F1:助動詞,F_F5:特殊・タ,F_F6:基本形,A:基本形,B:動詞-自立,F_EOS:1
渡し    動詞,自立,*,*,五段・サ行,連用形,渡す,ワタシ,ワタシ
た      助動詞,*,*,*,特殊・タ,基本形,た,タ,タ
EOS
```

#### 係り受け解析レイヤの出力フォーマット ####
文節切りフォーマットでは, 係り先が常に -1でしたが, これらが同定されます。また、文節切りフォーマットの他に, さきほど説明した 主辞/機能語の位置と係り関係のスコアが追加されます。係り関係のスコアは, 係りやすさの度合を示します. 一般に大きな値ほど係りやすいことを表します。しかし, 各スコアと実際の解析精度の関係は調査中です。このスコアの意味づけに関しては調査する必要があると考えています。

```
太郎は花子が読んでいる本を次郎に渡した
* 0 5D 0/1 1.228564
太郎    名詞,固有名詞,人名,名,*,*,太郎,タロウ,タロー
は      助詞,係助詞,*,*,*,*,は,ハ,ワ
* 1 2D 0/1 1.734436
花子    名詞,固有名詞,人名,名,*,*,花子,ハナコ,ハナコ
が      助詞,格助詞,一般,*,*,*,が,ガ,ガ
* 2 3D 0/2 0.000000
読ん    動詞,自立,*,*,五段・マ行,連用タ接続,読む,ヨン,ヨン
で      助詞,接続助詞,*,*,*,*,で,デ,デ
いる    動詞,非自立,*,*,一段,基本形,いる,イル,イル
* 3 5D 0/1 0.000000
本      名詞,一般,*,*,*,*,本,ホン,ホン
を      助詞,格助詞,一般,*,*,*,を,ヲ,ヲ
* 4 5D 1/2 0.000000
次      名詞,一般,*,*,*,*,次,ツギ,ツギ
郎      名詞,一般,*,*,*,*,郎,ロウ,ロー
に      助詞,格助詞,一般,*,*,*,に,ニ,ニ
* 5 -1D 0/1 0.000000
渡し    動詞,自立,*,*,五段・サ行,連用形,渡す,ワタシ,ワタシ
た      助動詞,*,*,*,特殊・タ,基本形,た,タ,タ
EOS
```

## 固有表現解析 ##
-n オプションで, IREX(Information Retrieval and Extraction Exercis)の 定義に基づく固有表現タグを出力します。 解析精度は F値で 85 ポイント前後だ と思います。固有表現解析には, 以下の 3 つのモードが存在します. 適宜使いわけてください。

  * -n 0:  固有表現解析を行わない
  * -n 1:  固有表現解析を行う。ただし, 文節の整合性を保つ. (デフォルト)  文節の整合性とは, 「1固有表現が, 複数の文節で構成されない」 という制約です。
  * -n 2:  固有表現解析を行う。 ただし文節の整合性を保たない。

-f1 オプションを用いた場合, 固有表現は, IOB2形式で出力されます.
  * B-X:  Xという固有表現の開始
  * I-X:  Xという固有表現の開始以外
  * O:    固有表現以外

以下が, 具体例です. 活用型, 活用形の後に 固有表現タグが付与されます。
```
% cabocha -f1 -n1
太郎と花子は2003年奈良先端大を卒業した
* 0 1D 0/1 1.230433
太郎    名詞,固有名詞,人名,名,*,*,太郎,タロウ,タロー    B-PERSON
と      助詞,並立助詞,*,*,*,*,と,ト,ト  O
* 1 4D 0/1 0.830090
花子    名詞,固有名詞,人名,名,*,*,花子,ハナコ,ハナコ    B-PERSON
は      助詞,係助詞,*,*,*,*,は,ハ,ワ    O
* 2 3D 1/1 0.381847
2003    名詞,数,*,*,*,*,*       B-DATE
年      名詞,接尾,助数詞,*,*,*,年,ネン,ネン     I-DATE
* 3 4D 0/1 0.000000
奈良先端大      名詞,固有名詞,組織,*,*,*,奈良先端大,ナラセンタンダイ,ナラセンタンダイ   B-ORGANIZATION
を      助詞,格助詞,一般,*,*,*,を,ヲ,ヲ O
* 4 -1D 1/2 0.000000
卒業    名詞,サ変接続,*,*,*,*,卒業,ソツギョウ,ソツギョー        O
し      動詞,自立,*,*,サ変・スル,連用形,する,シ,シ      O
た      助動詞,*,*,*,特殊・タ,基本形,た,タ,タ   O
EOS

```

### コマンドラインオプション ###
```
 -f, --output-format=TYPE  set output format style
                            0 - tree(default)
                            1 - lattice
                            2 - tree + lattice
                            3 - XML
 -I, --input-layer=LAYER   set input layer
                            0 - raw sentence layer(default)
                            1 - POS tagged layer
                            2 - POS tagger and Chunked layer
                            3 - POS tagged, Chunked and Feature selected layer
 -O, --output-layer=LAYER  set output layer
                            1 - POS tagged layer
                            2 - POS tagged and Chunked layer
                            3 - POS tagged, Chunked and Feature selected layer
                            4 - Parsed layer(default)
 -n, --ne=MODE             output NE tag
                            0 - without NE(default)
                            1 - output NE with chunk constraint
                            2 - output NE without chunk constraint
 -m, --parser-model=FILE   use FILE as parser model file
 -M, --chunker-model=FILE  use FILE as chunker model file
 -N, --ne-model=FILE       use FILE as NE tagger model file
 -P, --posset=STR          make posset of morphological analyzer (default IPA)
 -t, --charset=ENC         make charset of binary dictionary ENC (default UTF-8)
 -T, --charset-file=FILE   use charset written in FILE
 -r, --rcfile=FILE         use FILE as resource file
 -b, --mecabrc=FILE        use FILE as mecabrc
 -d, --mecab-dicdir=DIR    use DIR as mecab dictionary directory
 -o, --output=FILE         use FILE as output file
 -v, --version             show the version and exit
 -h, --help                show this help and exit
```

## 学習/評価 ##
#### 学習 ####
学習データを用意すれば、係り受け解析・文節区切りの学習が行えます。基本的に各レイヤの出力と同じ形式で学習データを構築し、cabocha-learn (/usr/local/libexec/cabocha/cabocha-learn) でモデルを構築します。
```
% cabocha-learn -e <mode> -P <tagset> -t <charset> <train file> <model file>
```
  * mode: dep, chunk, ne のいずれか
  * tagset: JUMAN , IPA, もしくは UNIDIC
  * charset: train fileの文字コード
  * train file: 学習データ
  * model file: モデルファイル

例
```
% cabocha-learn -e dep -P JUMAN -t UTF8 train.cab model
```

#### 再学習 ####
係り受け解析の学習に限り、再学習が行えます。再学習とは、少量の追加学習データと既存のモデルファイルから新しモデルを構築することを指します。再学習には -M オプションで現在のモデルファイル(テキスト形式)を指定します。

例
```
% cabocha-learn -e dep - P JUMAN -t UTF8 -M model/dep.juman.txt train-new.cab new_model
```


#### 京都大学テキストコーパスからの学習 ####
  * tools/kc2juman.pl   京都大学テキストコーパスをCaboCha形式(JUMAN品詞体系)の学習データに変換します。
  * tools/kcs2cabocha.pl  京都大学テキストコーパスをCaboCha形式(IPA品詞体系, もしくは Unidic品詞体系)の学習データに変換します。実行にはMeCabのperlモジュールが必要です。

#### テキストモデルとバイナリモデル ####
通常、学習後できあがるモデルファイルはアーキテクチャ依存のバイナリモデルファイルです。モデルを配布する場合は、テキスト形式のモデルファイルを扱う方が便利です。テキスト形式は、.txt という拡張子が付与されています。cabocha-model-index (/usr/local/libexec/cabocha/cabocha-model-index) でテキスト形式のモデルをバイナリ形式に変換することができます

```
% cabocha-model-index -f <source charset> -t <target charset> <text model> <binary model>
```

例 (UTF-8で保存されたテキストモデルを EUC形式のバイナリモデルに変換)
```
% cabocha-model-index -f UTF8 -t EUC model.txt model
```

#### 評価 ####
cabocha-system-eval を使います
```
% cabocha-system-eval -e <mode> <result> <golden>
```
  * mode: dep, chunk もしくは ne
  * result: システムの出力結果
  * golden: 正解データ

```
% cabocha-system-eval -e dep result golden
dependency level1: 90.9455 (73293/80590)
dependency level2: 89.7824 (64119/71416)
sentence         : 54.4162 (5052/9284)
```
  * level1: 最後の文節を取り除いた文節係り受け正解率
  * level2: 最後と最後から2番目の文節を取り除いた文節係り受け正解率
  * sentence: 文レベルの正解率

## [CaboCha](http://code.google.com/p/cabocha/)に関する発表 ##
  * Taku Kudo, Yuji Matsumoto (2003)     Fast Methods for Kernel-Based Text Analysis, ACL 2003 in Sapporo, Japan
  * Taku Kudo, Yuji Matsumoto (2002)    Japanese Dependency Analyisis using Cascaded Chunking, CONLL 2002 in TAIPEI
  * 工藤 拓, 松本 裕治 (2002)   チャンキングの段階適用による係り受け解析, 情報処理学会
  * 工藤 拓, 松本 裕治 (2001)   チャンキングの段階適用による係り受け解析, SIG-NL-142
  * 工藤 拓, 松本 裕治 (2000)   Support Vector Machine による日本語係り受け解析 SIG-NL-138
  * 工藤 拓, 松本 裕治 (2001)   Cascaded Chunking Model における部分解析済み情報の利用 情報処理学会全国大会 2001
  * Taku Kudoh, Yuji Matsumoto (2000)  Japanese Dependency Analysis Based on Support Vector Machines,EMNLP/VLC 2000

## 謝辞 ##
  * 株式会社毎日新聞社の御厚意により毎日新聞記事CD-ROM版に関して研究目的での使用承諾をいただきました。ここに感謝いたします。
  * 京都大学コーパスを作成し無償で公開なさっている 京都大学言語メディア研究室の皆さまに感謝いたします。
  * 特に同コーパス作成の中心的役割を果たしていらっしゃる黒橋禎夫 助教授に感謝いたします。
  * IREX実行委員会のみなさまに感謝いたします。
  * IPA 品詞体系における主辞,機能語ルールを作成していただたいた 松本 裕治 教授に感謝いたします。
  * JUMAN 並びに ChaSen の開発者の方々に感謝いたします。
  * CaboCha の開発段階において多くのコメントをいただいた松本研究室の皆さまに感謝します。