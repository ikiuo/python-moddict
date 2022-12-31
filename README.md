# ModDict

規則性の低い数値の一覧を対象とした読取り専用辞書です。辞書の参照キーは 32 ビットの符号なし整数です。各キーに対する剰余が一意となる除数を使って値を取得します。条件を絞ることで dict よりも高速な処理が期待できます。

例として、辞書を

> mapping = {<br>
> &nbsp;&nbsp;&nbsp;&nbsp; 57183:0, 26302:1, 12855:2, 61489:3, 21256:4,<br/>
> &nbsp;&nbsp;&nbsp;&nbsp; 14532:5, 24168:6, 37683:7, 18386:8, 35798:9,<br/>
> &nbsp;&nbsp;&nbsp;&nbsp; 7211:10,<br/>
> }

とすると、キーと値は

> keys = [57183, 26302, 12855, 61489, 21256, 14532, 24168, 37683, 18386, 35798, 7211]<br/>
> values = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10]

keys に対応する剰余が一意となる除数の候補では 45 が存在するので

> divisor = 45<br/>
> remainders = [n % divisor for n in keys]<br/>
> \# = [33, 22, 30, 19, 16, 42, 3, 18, 26, 23, 11]<br/>

剰余に対応する値の一覧は

> mkvalues = [<br/>
> &nbsp;&nbsp;&nbsp;&nbsp; None, None, None, 6, None, None, None, None, None, None,<br/>
> &nbsp;&nbsp;&nbsp;&nbsp; None, 10, None, None, None, None, 4, None, 7, 3,<br/>
> &nbsp;&nbsp;&nbsp;&nbsp; None, None, 1, 9, None, None, 8, None, None, None,<br/>
> &nbsp;&nbsp;&nbsp;&nbsp; 2, None, None, 0, None, None, None, None, None, None,<br/>
> &nbsp;&nbsp;&nbsp;&nbsp; None, None,  5, None, None<br/>
> ] # 辞書にないものは None としている

とすることができます。

キーに対する値の取得は

> value = mkvalues[key % divisor]

とすることができます。同様にキーも

> modkeys = [<br/>
> &nbsp;&nbsp;&nbsp;&nbsp; None, None, None, 24168, None, None, None, None, None, None,<br/>
> &nbsp;&nbsp;&nbsp;&nbsp; None, 7211, None, None, None, None, 21256, None, 37683, 61489,<br/>
> &nbsp;&nbsp;&nbsp;&nbsp; None, None, 26302, 35798, None, None, 18386, None, None, None,<br/>
> &nbsp;&nbsp;&nbsp;&nbsp; 12855, None, None, 57183, None, None, None, None, None, None,<br/>
> &nbsp;&nbsp;&nbsp;&nbsp; None, None,  14532, None, None<br/>
> ] # 辞書にないものは None としている

を用意してき

> key == modkeys[key % divisor]

でキーの有効性を確認できます。

## コンストラクタ

除数を求める処理は遅いので、ModDict オブジェクトの生成には時間がかかります。

### ModDict(mapping)

mapping オブジェクトから ModDict オブジェクトを生成します。

### ModDict(iteratable)

数列から ModDict オブジェクトを生成します。<br/>値は iteratable の順に番号を割り当てます。

### ModDict(iteratable, value)

数列と値から ModDict オブジェクトを生成します。<br/>値は value のみになります。

## メソッド

### divisor()

キーに対する除数を返します。<br/>逆引き表が生成されていない場合は None を返します。

### modkeys()

剰余に対応するキーの一覧を返します。

### mkvalues()

剰余に対応する辞書の値の一覧を返します。

### remainder_index()

剰余に対する keys(), values(), items() へのインデックス一覧を返します。

### get(key [,default])

key に対する値を返します。<br/>key が存在しない場合は default を返します。<br/>default に指定がない場合は None を返します。

### keys()

辞書内の全てのキーを list として返します。<br/><small>(PyDict_Keys 関数を使用)</small>

### values()

辞書内の全ての値を list として返します。<br/><small>(PyDict_Values 関数を使用)</small>

### items()

辞書内の全ての (キー, 値) を list として返します。<br/><small>(PyDict_Items 関数を使用)</small>

### dict()

ModDict が保持している dict のコピーを返します。<br/><small>(PyDict_Copy 関数を使用)</small>

## クラスメソッド

### forindex(keys)

ModDict(keys) を返します。
