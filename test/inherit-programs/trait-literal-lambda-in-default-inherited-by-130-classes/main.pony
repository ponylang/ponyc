// 130 classes each inherit T.mk, whose lambda captures a, with 130 different
// type arguments. Each lambda prints p; expects 130. The program must compile
// with one lambda class per inheritor.
trait T[A: Stringable val]
  fun mk(a: A): {(): String} val => {()(a): String => a.string()} val
primitive P1 is Stringable
  fun string(): String iso^ => "p".clone()
class C1 is T[P1]
  new create() => None
primitive P2 is Stringable
  fun string(): String iso^ => "p".clone()
class C2 is T[P2]
  new create() => None
primitive P3 is Stringable
  fun string(): String iso^ => "p".clone()
class C3 is T[P3]
  new create() => None
primitive P4 is Stringable
  fun string(): String iso^ => "p".clone()
class C4 is T[P4]
  new create() => None
primitive P5 is Stringable
  fun string(): String iso^ => "p".clone()
class C5 is T[P5]
  new create() => None
primitive P6 is Stringable
  fun string(): String iso^ => "p".clone()
class C6 is T[P6]
  new create() => None
primitive P7 is Stringable
  fun string(): String iso^ => "p".clone()
class C7 is T[P7]
  new create() => None
primitive P8 is Stringable
  fun string(): String iso^ => "p".clone()
class C8 is T[P8]
  new create() => None
primitive P9 is Stringable
  fun string(): String iso^ => "p".clone()
class C9 is T[P9]
  new create() => None
primitive P10 is Stringable
  fun string(): String iso^ => "p".clone()
class C10 is T[P10]
  new create() => None
primitive P11 is Stringable
  fun string(): String iso^ => "p".clone()
class C11 is T[P11]
  new create() => None
primitive P12 is Stringable
  fun string(): String iso^ => "p".clone()
class C12 is T[P12]
  new create() => None
primitive P13 is Stringable
  fun string(): String iso^ => "p".clone()
class C13 is T[P13]
  new create() => None
primitive P14 is Stringable
  fun string(): String iso^ => "p".clone()
class C14 is T[P14]
  new create() => None
primitive P15 is Stringable
  fun string(): String iso^ => "p".clone()
class C15 is T[P15]
  new create() => None
primitive P16 is Stringable
  fun string(): String iso^ => "p".clone()
class C16 is T[P16]
  new create() => None
primitive P17 is Stringable
  fun string(): String iso^ => "p".clone()
class C17 is T[P17]
  new create() => None
primitive P18 is Stringable
  fun string(): String iso^ => "p".clone()
class C18 is T[P18]
  new create() => None
primitive P19 is Stringable
  fun string(): String iso^ => "p".clone()
class C19 is T[P19]
  new create() => None
primitive P20 is Stringable
  fun string(): String iso^ => "p".clone()
class C20 is T[P20]
  new create() => None
primitive P21 is Stringable
  fun string(): String iso^ => "p".clone()
class C21 is T[P21]
  new create() => None
primitive P22 is Stringable
  fun string(): String iso^ => "p".clone()
class C22 is T[P22]
  new create() => None
primitive P23 is Stringable
  fun string(): String iso^ => "p".clone()
class C23 is T[P23]
  new create() => None
primitive P24 is Stringable
  fun string(): String iso^ => "p".clone()
class C24 is T[P24]
  new create() => None
primitive P25 is Stringable
  fun string(): String iso^ => "p".clone()
class C25 is T[P25]
  new create() => None
primitive P26 is Stringable
  fun string(): String iso^ => "p".clone()
class C26 is T[P26]
  new create() => None
primitive P27 is Stringable
  fun string(): String iso^ => "p".clone()
class C27 is T[P27]
  new create() => None
primitive P28 is Stringable
  fun string(): String iso^ => "p".clone()
class C28 is T[P28]
  new create() => None
primitive P29 is Stringable
  fun string(): String iso^ => "p".clone()
class C29 is T[P29]
  new create() => None
primitive P30 is Stringable
  fun string(): String iso^ => "p".clone()
class C30 is T[P30]
  new create() => None
primitive P31 is Stringable
  fun string(): String iso^ => "p".clone()
class C31 is T[P31]
  new create() => None
primitive P32 is Stringable
  fun string(): String iso^ => "p".clone()
class C32 is T[P32]
  new create() => None
primitive P33 is Stringable
  fun string(): String iso^ => "p".clone()
class C33 is T[P33]
  new create() => None
primitive P34 is Stringable
  fun string(): String iso^ => "p".clone()
class C34 is T[P34]
  new create() => None
primitive P35 is Stringable
  fun string(): String iso^ => "p".clone()
class C35 is T[P35]
  new create() => None
primitive P36 is Stringable
  fun string(): String iso^ => "p".clone()
class C36 is T[P36]
  new create() => None
primitive P37 is Stringable
  fun string(): String iso^ => "p".clone()
class C37 is T[P37]
  new create() => None
primitive P38 is Stringable
  fun string(): String iso^ => "p".clone()
class C38 is T[P38]
  new create() => None
primitive P39 is Stringable
  fun string(): String iso^ => "p".clone()
class C39 is T[P39]
  new create() => None
primitive P40 is Stringable
  fun string(): String iso^ => "p".clone()
class C40 is T[P40]
  new create() => None
primitive P41 is Stringable
  fun string(): String iso^ => "p".clone()
class C41 is T[P41]
  new create() => None
primitive P42 is Stringable
  fun string(): String iso^ => "p".clone()
class C42 is T[P42]
  new create() => None
primitive P43 is Stringable
  fun string(): String iso^ => "p".clone()
class C43 is T[P43]
  new create() => None
primitive P44 is Stringable
  fun string(): String iso^ => "p".clone()
class C44 is T[P44]
  new create() => None
primitive P45 is Stringable
  fun string(): String iso^ => "p".clone()
class C45 is T[P45]
  new create() => None
primitive P46 is Stringable
  fun string(): String iso^ => "p".clone()
class C46 is T[P46]
  new create() => None
primitive P47 is Stringable
  fun string(): String iso^ => "p".clone()
class C47 is T[P47]
  new create() => None
primitive P48 is Stringable
  fun string(): String iso^ => "p".clone()
class C48 is T[P48]
  new create() => None
primitive P49 is Stringable
  fun string(): String iso^ => "p".clone()
class C49 is T[P49]
  new create() => None
primitive P50 is Stringable
  fun string(): String iso^ => "p".clone()
class C50 is T[P50]
  new create() => None
primitive P51 is Stringable
  fun string(): String iso^ => "p".clone()
class C51 is T[P51]
  new create() => None
primitive P52 is Stringable
  fun string(): String iso^ => "p".clone()
class C52 is T[P52]
  new create() => None
primitive P53 is Stringable
  fun string(): String iso^ => "p".clone()
class C53 is T[P53]
  new create() => None
primitive P54 is Stringable
  fun string(): String iso^ => "p".clone()
class C54 is T[P54]
  new create() => None
primitive P55 is Stringable
  fun string(): String iso^ => "p".clone()
class C55 is T[P55]
  new create() => None
primitive P56 is Stringable
  fun string(): String iso^ => "p".clone()
class C56 is T[P56]
  new create() => None
primitive P57 is Stringable
  fun string(): String iso^ => "p".clone()
class C57 is T[P57]
  new create() => None
primitive P58 is Stringable
  fun string(): String iso^ => "p".clone()
class C58 is T[P58]
  new create() => None
primitive P59 is Stringable
  fun string(): String iso^ => "p".clone()
class C59 is T[P59]
  new create() => None
primitive P60 is Stringable
  fun string(): String iso^ => "p".clone()
class C60 is T[P60]
  new create() => None
primitive P61 is Stringable
  fun string(): String iso^ => "p".clone()
class C61 is T[P61]
  new create() => None
primitive P62 is Stringable
  fun string(): String iso^ => "p".clone()
class C62 is T[P62]
  new create() => None
primitive P63 is Stringable
  fun string(): String iso^ => "p".clone()
class C63 is T[P63]
  new create() => None
primitive P64 is Stringable
  fun string(): String iso^ => "p".clone()
class C64 is T[P64]
  new create() => None
primitive P65 is Stringable
  fun string(): String iso^ => "p".clone()
class C65 is T[P65]
  new create() => None
primitive P66 is Stringable
  fun string(): String iso^ => "p".clone()
class C66 is T[P66]
  new create() => None
primitive P67 is Stringable
  fun string(): String iso^ => "p".clone()
class C67 is T[P67]
  new create() => None
primitive P68 is Stringable
  fun string(): String iso^ => "p".clone()
class C68 is T[P68]
  new create() => None
primitive P69 is Stringable
  fun string(): String iso^ => "p".clone()
class C69 is T[P69]
  new create() => None
primitive P70 is Stringable
  fun string(): String iso^ => "p".clone()
class C70 is T[P70]
  new create() => None
primitive P71 is Stringable
  fun string(): String iso^ => "p".clone()
class C71 is T[P71]
  new create() => None
primitive P72 is Stringable
  fun string(): String iso^ => "p".clone()
class C72 is T[P72]
  new create() => None
primitive P73 is Stringable
  fun string(): String iso^ => "p".clone()
class C73 is T[P73]
  new create() => None
primitive P74 is Stringable
  fun string(): String iso^ => "p".clone()
class C74 is T[P74]
  new create() => None
primitive P75 is Stringable
  fun string(): String iso^ => "p".clone()
class C75 is T[P75]
  new create() => None
primitive P76 is Stringable
  fun string(): String iso^ => "p".clone()
class C76 is T[P76]
  new create() => None
primitive P77 is Stringable
  fun string(): String iso^ => "p".clone()
class C77 is T[P77]
  new create() => None
primitive P78 is Stringable
  fun string(): String iso^ => "p".clone()
class C78 is T[P78]
  new create() => None
primitive P79 is Stringable
  fun string(): String iso^ => "p".clone()
class C79 is T[P79]
  new create() => None
primitive P80 is Stringable
  fun string(): String iso^ => "p".clone()
class C80 is T[P80]
  new create() => None
primitive P81 is Stringable
  fun string(): String iso^ => "p".clone()
class C81 is T[P81]
  new create() => None
primitive P82 is Stringable
  fun string(): String iso^ => "p".clone()
class C82 is T[P82]
  new create() => None
primitive P83 is Stringable
  fun string(): String iso^ => "p".clone()
class C83 is T[P83]
  new create() => None
primitive P84 is Stringable
  fun string(): String iso^ => "p".clone()
class C84 is T[P84]
  new create() => None
primitive P85 is Stringable
  fun string(): String iso^ => "p".clone()
class C85 is T[P85]
  new create() => None
primitive P86 is Stringable
  fun string(): String iso^ => "p".clone()
class C86 is T[P86]
  new create() => None
primitive P87 is Stringable
  fun string(): String iso^ => "p".clone()
class C87 is T[P87]
  new create() => None
primitive P88 is Stringable
  fun string(): String iso^ => "p".clone()
class C88 is T[P88]
  new create() => None
primitive P89 is Stringable
  fun string(): String iso^ => "p".clone()
class C89 is T[P89]
  new create() => None
primitive P90 is Stringable
  fun string(): String iso^ => "p".clone()
class C90 is T[P90]
  new create() => None
primitive P91 is Stringable
  fun string(): String iso^ => "p".clone()
class C91 is T[P91]
  new create() => None
primitive P92 is Stringable
  fun string(): String iso^ => "p".clone()
class C92 is T[P92]
  new create() => None
primitive P93 is Stringable
  fun string(): String iso^ => "p".clone()
class C93 is T[P93]
  new create() => None
primitive P94 is Stringable
  fun string(): String iso^ => "p".clone()
class C94 is T[P94]
  new create() => None
primitive P95 is Stringable
  fun string(): String iso^ => "p".clone()
class C95 is T[P95]
  new create() => None
primitive P96 is Stringable
  fun string(): String iso^ => "p".clone()
class C96 is T[P96]
  new create() => None
primitive P97 is Stringable
  fun string(): String iso^ => "p".clone()
class C97 is T[P97]
  new create() => None
primitive P98 is Stringable
  fun string(): String iso^ => "p".clone()
class C98 is T[P98]
  new create() => None
primitive P99 is Stringable
  fun string(): String iso^ => "p".clone()
class C99 is T[P99]
  new create() => None
primitive P100 is Stringable
  fun string(): String iso^ => "p".clone()
class C100 is T[P100]
  new create() => None
primitive P101 is Stringable
  fun string(): String iso^ => "p".clone()
class C101 is T[P101]
  new create() => None
primitive P102 is Stringable
  fun string(): String iso^ => "p".clone()
class C102 is T[P102]
  new create() => None
primitive P103 is Stringable
  fun string(): String iso^ => "p".clone()
class C103 is T[P103]
  new create() => None
primitive P104 is Stringable
  fun string(): String iso^ => "p".clone()
class C104 is T[P104]
  new create() => None
primitive P105 is Stringable
  fun string(): String iso^ => "p".clone()
class C105 is T[P105]
  new create() => None
primitive P106 is Stringable
  fun string(): String iso^ => "p".clone()
class C106 is T[P106]
  new create() => None
primitive P107 is Stringable
  fun string(): String iso^ => "p".clone()
class C107 is T[P107]
  new create() => None
primitive P108 is Stringable
  fun string(): String iso^ => "p".clone()
class C108 is T[P108]
  new create() => None
primitive P109 is Stringable
  fun string(): String iso^ => "p".clone()
class C109 is T[P109]
  new create() => None
primitive P110 is Stringable
  fun string(): String iso^ => "p".clone()
class C110 is T[P110]
  new create() => None
primitive P111 is Stringable
  fun string(): String iso^ => "p".clone()
class C111 is T[P111]
  new create() => None
primitive P112 is Stringable
  fun string(): String iso^ => "p".clone()
class C112 is T[P112]
  new create() => None
primitive P113 is Stringable
  fun string(): String iso^ => "p".clone()
class C113 is T[P113]
  new create() => None
primitive P114 is Stringable
  fun string(): String iso^ => "p".clone()
class C114 is T[P114]
  new create() => None
primitive P115 is Stringable
  fun string(): String iso^ => "p".clone()
class C115 is T[P115]
  new create() => None
primitive P116 is Stringable
  fun string(): String iso^ => "p".clone()
class C116 is T[P116]
  new create() => None
primitive P117 is Stringable
  fun string(): String iso^ => "p".clone()
class C117 is T[P117]
  new create() => None
primitive P118 is Stringable
  fun string(): String iso^ => "p".clone()
class C118 is T[P118]
  new create() => None
primitive P119 is Stringable
  fun string(): String iso^ => "p".clone()
class C119 is T[P119]
  new create() => None
primitive P120 is Stringable
  fun string(): String iso^ => "p".clone()
class C120 is T[P120]
  new create() => None
primitive P121 is Stringable
  fun string(): String iso^ => "p".clone()
class C121 is T[P121]
  new create() => None
primitive P122 is Stringable
  fun string(): String iso^ => "p".clone()
class C122 is T[P122]
  new create() => None
primitive P123 is Stringable
  fun string(): String iso^ => "p".clone()
class C123 is T[P123]
  new create() => None
primitive P124 is Stringable
  fun string(): String iso^ => "p".clone()
class C124 is T[P124]
  new create() => None
primitive P125 is Stringable
  fun string(): String iso^ => "p".clone()
class C125 is T[P125]
  new create() => None
primitive P126 is Stringable
  fun string(): String iso^ => "p".clone()
class C126 is T[P126]
  new create() => None
primitive P127 is Stringable
  fun string(): String iso^ => "p".clone()
class C127 is T[P127]
  new create() => None
primitive P128 is Stringable
  fun string(): String iso^ => "p".clone()
class C128 is T[P128]
  new create() => None
primitive P129 is Stringable
  fun string(): String iso^ => "p".clone()
class C129 is T[P129]
  new create() => None
primitive P130 is Stringable
  fun string(): String iso^ => "p".clone()
class C130 is T[P130]
  new create() => None
actor Main
  new create(env: Env) =>
    var n: USize = 0
    n = n + C1.mk(P1)().size()
    n = n + C2.mk(P2)().size()
    n = n + C3.mk(P3)().size()
    n = n + C4.mk(P4)().size()
    n = n + C5.mk(P5)().size()
    n = n + C6.mk(P6)().size()
    n = n + C7.mk(P7)().size()
    n = n + C8.mk(P8)().size()
    n = n + C9.mk(P9)().size()
    n = n + C10.mk(P10)().size()
    n = n + C11.mk(P11)().size()
    n = n + C12.mk(P12)().size()
    n = n + C13.mk(P13)().size()
    n = n + C14.mk(P14)().size()
    n = n + C15.mk(P15)().size()
    n = n + C16.mk(P16)().size()
    n = n + C17.mk(P17)().size()
    n = n + C18.mk(P18)().size()
    n = n + C19.mk(P19)().size()
    n = n + C20.mk(P20)().size()
    n = n + C21.mk(P21)().size()
    n = n + C22.mk(P22)().size()
    n = n + C23.mk(P23)().size()
    n = n + C24.mk(P24)().size()
    n = n + C25.mk(P25)().size()
    n = n + C26.mk(P26)().size()
    n = n + C27.mk(P27)().size()
    n = n + C28.mk(P28)().size()
    n = n + C29.mk(P29)().size()
    n = n + C30.mk(P30)().size()
    n = n + C31.mk(P31)().size()
    n = n + C32.mk(P32)().size()
    n = n + C33.mk(P33)().size()
    n = n + C34.mk(P34)().size()
    n = n + C35.mk(P35)().size()
    n = n + C36.mk(P36)().size()
    n = n + C37.mk(P37)().size()
    n = n + C38.mk(P38)().size()
    n = n + C39.mk(P39)().size()
    n = n + C40.mk(P40)().size()
    n = n + C41.mk(P41)().size()
    n = n + C42.mk(P42)().size()
    n = n + C43.mk(P43)().size()
    n = n + C44.mk(P44)().size()
    n = n + C45.mk(P45)().size()
    n = n + C46.mk(P46)().size()
    n = n + C47.mk(P47)().size()
    n = n + C48.mk(P48)().size()
    n = n + C49.mk(P49)().size()
    n = n + C50.mk(P50)().size()
    n = n + C51.mk(P51)().size()
    n = n + C52.mk(P52)().size()
    n = n + C53.mk(P53)().size()
    n = n + C54.mk(P54)().size()
    n = n + C55.mk(P55)().size()
    n = n + C56.mk(P56)().size()
    n = n + C57.mk(P57)().size()
    n = n + C58.mk(P58)().size()
    n = n + C59.mk(P59)().size()
    n = n + C60.mk(P60)().size()
    n = n + C61.mk(P61)().size()
    n = n + C62.mk(P62)().size()
    n = n + C63.mk(P63)().size()
    n = n + C64.mk(P64)().size()
    n = n + C65.mk(P65)().size()
    n = n + C66.mk(P66)().size()
    n = n + C67.mk(P67)().size()
    n = n + C68.mk(P68)().size()
    n = n + C69.mk(P69)().size()
    n = n + C70.mk(P70)().size()
    n = n + C71.mk(P71)().size()
    n = n + C72.mk(P72)().size()
    n = n + C73.mk(P73)().size()
    n = n + C74.mk(P74)().size()
    n = n + C75.mk(P75)().size()
    n = n + C76.mk(P76)().size()
    n = n + C77.mk(P77)().size()
    n = n + C78.mk(P78)().size()
    n = n + C79.mk(P79)().size()
    n = n + C80.mk(P80)().size()
    n = n + C81.mk(P81)().size()
    n = n + C82.mk(P82)().size()
    n = n + C83.mk(P83)().size()
    n = n + C84.mk(P84)().size()
    n = n + C85.mk(P85)().size()
    n = n + C86.mk(P86)().size()
    n = n + C87.mk(P87)().size()
    n = n + C88.mk(P88)().size()
    n = n + C89.mk(P89)().size()
    n = n + C90.mk(P90)().size()
    n = n + C91.mk(P91)().size()
    n = n + C92.mk(P92)().size()
    n = n + C93.mk(P93)().size()
    n = n + C94.mk(P94)().size()
    n = n + C95.mk(P95)().size()
    n = n + C96.mk(P96)().size()
    n = n + C97.mk(P97)().size()
    n = n + C98.mk(P98)().size()
    n = n + C99.mk(P99)().size()
    n = n + C100.mk(P100)().size()
    n = n + C101.mk(P101)().size()
    n = n + C102.mk(P102)().size()
    n = n + C103.mk(P103)().size()
    n = n + C104.mk(P104)().size()
    n = n + C105.mk(P105)().size()
    n = n + C106.mk(P106)().size()
    n = n + C107.mk(P107)().size()
    n = n + C108.mk(P108)().size()
    n = n + C109.mk(P109)().size()
    n = n + C110.mk(P110)().size()
    n = n + C111.mk(P111)().size()
    n = n + C112.mk(P112)().size()
    n = n + C113.mk(P113)().size()
    n = n + C114.mk(P114)().size()
    n = n + C115.mk(P115)().size()
    n = n + C116.mk(P116)().size()
    n = n + C117.mk(P117)().size()
    n = n + C118.mk(P118)().size()
    n = n + C119.mk(P119)().size()
    n = n + C120.mk(P120)().size()
    n = n + C121.mk(P121)().size()
    n = n + C122.mk(P122)().size()
    n = n + C123.mk(P123)().size()
    n = n + C124.mk(P124)().size()
    n = n + C125.mk(P125)().size()
    n = n + C126.mk(P126)().size()
    n = n + C127.mk(P127)().size()
    n = n + C128.mk(P128)().size()
    n = n + C129.mk(P129)().size()
    n = n + C130.mk(P130)().size()
    env.out.print(n.string())
