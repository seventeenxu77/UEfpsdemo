# UE FPS Demo — 多人竞争刷怪射击

基于 **Unreal Engine 5.7** 官方 First Person 模板，从零自建的第一人称射击 Demo。

## 玩法

多名玩家联机进入同一场地，面对持续刷新的 AI 敌人，**比谁击杀的 AI 更多**——先达到目标击杀数者获胜。玩家之间可互相射击（死亡后自动重生），但只有击杀 AI 才计分。

## 实现的三大功能

1. **会移动 / 攻击 / 可击败的敌人** —— `AFPSEnemy` + `AFPSEnemyAIController`：NavMesh 寻路追击、近战 + 远程射击、可被击杀并布娃娃倒地；`AFPSEnemySpawner` 在导航网格随机点持续刷怪、维持场上数量；AI 自动锁定**最近的存活玩家**。
2. **得分 + 胜利机制** —— 击杀 AI 给凶手玩家记分（`AFPSPlayerState`），先到 `ScoreToWin` 者胜（`AFPSGameState`）；`AFPSHUD` 实时显示血量、击杀进度与胜负界面。
3. **多人网络对战** —— 服务器权威架构：角色与敌人全部复制；开火走 `Server` RPC + 服务器射线判定；受击/开火/死亡等表现走 `NetMulticast`；血量、计分、比赛状态全部复制；玩家死亡由服务器在 `PlayerStart` 重生。

## 运行方式

1. 用 UE 5.7 打开 `mmmfps.uproject`（首次会提示编译 C++ 模块，确认即可）。
2. 编辑器内 Play 设置：**Number of Players ≥ 2**，**Net Mode = Play As Listen Server**。
3. 或命令行开局域网房间：
   - 房主：`UnrealEditor.exe mmmfps.uproject /Game/FirstPerson/Lvl_FirstPerson -game ?listen`
   - 加入：`UnrealEditor.exe mmmfps.uproject -game <房主IP>`

## 代码结构（`Source/mmmfps/`）

| 文件 | 职责 |
|---|---|
| `FPSCharacter.*` | 玩家：开火(Server RPC)/受击/死亡/重生、第一与第三人称双武器 |
| `FPSEnemy.*` / `FPSEnemyAIController.*` | 敌人身体与 AI（追击/近战/射击/最近玩家优先） |
| `FPSEnemySpawner.*` | 服务器侧导航网格随机刷怪 |
| `FPSGameMode.*` / `FPSGameState.*` / `FPSPlayerState.*` | 规则 / 比赛状态 / 玩家战绩 |
| `FPSHUD.*` | 准星、血量、击杀进度、胜负界面 |

> 渲染默认开启 Lumen / 光追 / 虚拟阴影（高画质）；联机测试时建议把 Engine Scalability 调低以保证帧率。
