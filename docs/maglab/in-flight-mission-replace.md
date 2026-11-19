# 飞行中航线替换需求（冻结草案）

## 目标

在 ArduPilot AUTO 任务执行期间，不暂停飞控，通过 MagLab 将新规划航线接入当前任务，并在上传完成后让飞控从指定新航点继续执行。

## 已确认决策

- 只支持 ArduPilot。
- 不暂停任务模式上传。
- 全量任务上传，不做部分更新。
- 桌面：双击 Fly View 航线进入。
- 移动端：长按航线或 MagLab 工具栏按钮进入，不使用双击。
- 上传前确认，不存在“上传后再确认执行”的暂存语义。
- 上传完成后留在 Plan View，不自动返回 Fly View。
- 不自动恢复或切换飞行模式。
- 不自动上传 LLM／Sketch 生成结果，必须用户确认。
- 首版不做 VTOL 转换阶段自动检测；确认框提示操作员自行确认。

## B1 拼接规则

在当前任务中读取 mission current，记为 `C`。

### 普通任务

保留任务项 `0..C`，新航线从 `C+1` 开始。

### 复杂任务内部

若 `C` 位于 Survey、Corridor Scan、LLM 或 Sketch 的序号范围内：

- 找到该复杂任务最后序号 `L`
- 保留完整的 `0..L`
- 新航线从 `L+1` 开始
- 上传后保持当前序号 `C`，让飞控完成当前复杂任务
- 新航线在复杂任务结束后生效

不拒绝上传，也不从复杂任务中间切断。

首版不开放“立即中断复杂任务”。后续如需开放，必须为不同复杂任务增加副作用清理和专门测试。

## 上传流程

1. 读取当前 mission index 和当前任务
2. 确认当前索引所属 visual item
3. 按普通／复杂边界生成拼接任务
4. 上传前显示：
   - 当前模式
   - 当前序号／总点数
   - 保留范围
   - 替换范围
   - 新航线首点
   - 上传后目标 current
5. 用户确认
6. 全量上传
7. 等待上传完成
8. 普通任务：发送 `DO_SET_MISSION_CURRENT = C+1`
9. 复杂任务：发送 `DO_SET_MISSION_CURRENT = C`，完成当前复杂任务后自然进入新航线
10. 校验 mission current
11. 从飞控重新同步本地任务模型，确保 Plan View／Fly View 显示与实际任务一致
12. 留在 Plan View

## 失败处理

- 上传失败：不设置 current
- 上传成功但设置 current 失败：提示任务已上传、执行序号未确认
- mission current 校验不一致：显示实际值，不自动重试
- 编辑期间 current 变化：重新生成拼接，重新确认；变化超出可保留范围则取消
- 上传期间链路断开：不自动恢复任务，保留错误信息

## 安全边界

- MagLab 专用上传路径只绕过 `SendToVehiclePreCheckStateActiveMission`
- 保留离线、同步中、高延迟链路、固件/机型不匹配等原有检查
- 不修改上游普通 Plan View 的上传策略
- 不自动暂停、恢复、返航或切换模式
- 不用测试包在真实飞行中验证；先使用 ArduPilot SITL 和地面测试

## 任务转换要求

- 新计划不得包含 Takeoff；发现时拒绝并提示
- 新计划的 Home 不得替换当前任务 Home
- 保留旧任务中已完成的前缀
- 新计划中的速度、相机、ROI 等 DO 指令顺序保持不变
- 新计划末尾 LAND 如存在则保留
- 重新编号所有任务项，检查连续序号和 `DO_JUMP` 目标
- 超过飞控任务数量限制时拒绝

## 待实现

- `src/Maglab/Flight/InFlightMissionReplaceController`
- `src/Maglab/Flight/InFlightMissionReplacePanel.qml`
- `src/Maglab/Flight/MissionRouteEntryArea.qml`
- 最小 Fly View 入口钩子，仅 `maglabModulesEnabled` 时启用
- SITL 测试：普通任务、复杂任务边界、索引变化、上传失败、设置 current 失败、DO_JUMP／速度／相机指令
- 目前未实现、未编译、未飞行验证

## 当前实现（开发中，未飞行验收）

已加入：

- `InFlightMissionReplaceController`：ArduPilot 校验、当前序号读取、B1 前缀/后缀拼接、复杂任务边界保留、全量上传、`DO_SET_MISSION_CURRENT`、超时和错误状态。
- `InFlightMissionReplacePanel.qml`：上传前预览、确认、上传和结果显示。
- `MissionRouteEntryArea.qml`：Fly View 航线双击/长按入口。
- Fly View 工具条“航线更新”按钮。
- Plan View 面板入口和本地任务重新同步。
- 新航线中的 Takeoff、DO_JUMP 首版拒绝；前缀必须与飞控任务一致。
- 已支持两种入口：
  - Fly View 双击／长按：只打开面板，不自动下载或上传；用户必须在面板里点击“读取飞控当前任务”，读取本身是下载，不会上传。读取后在 Plan View 修改后续航点。
  - Plan View 工具条的“飞行中替换”：把当前编辑计划作为独立新航线后缀，不覆盖工作区，旧前缀从飞控任务读取。
- 独立新航线模式使用 Fly View 的当前任务控制器识别复杂任务边界；如果没有该控制器则拒绝准备。
- 已在本机 ArduPilot Copter SITL 做初步协议级验证：
  - AUTO 中全量上传返回 `MISSION_ACK=0`，出现 `Flight plan received` 和 `Auto mission changed, restarted command`。
  - 上传后发送 `DO_SET_MISSION_CURRENT`，`MISSION_CURRENT` 变为目标序号。
  - 在一次 armed/AUTO 测试中，上传期间仍保持 armed；测试结束后 SITL 出现 `Disarming motors`，尚不能据此证明完整飞行任务可连续执行。
  - 未通过 QGC 图形界面或 MagLab 面板执行；未验证复杂任务边界、真实飞控、断链或拒收路径。

## 构建状态

- macOS Release 构建通过，`--simple-boot-test` 返回 0。
- Android arm64 Release 构建通过。
- Debug 签名测试 APK：`build/android-maglab-arm64/test-apk/Maglab-5.1.5-arm64-inflight-test.apk`
  SHA256 `af3dc48ecc88b627ed18ecbbe42fcdf29d415b1e3db21418685a66e13b6a2b04`（`Maglab-5.1.5-arm64-final-round-test.apk`）
- 该 APK 尚未安装；不能用于真实飞行验收。
- ## SITL 初步结果（ArduPilot Copter）

已在本机编译 ArduPilot Copter SITL，并通过 pymavlink 做协议级探测：

- AUTO 模式下全量任务上传被接受（`MISSION_ACK=0`），日志出现 `Flight plan received` 和 `Auto mission changed, restarted command`。
- `MAV_CMD_DO_SET_MISSION_CURRENT` 发送后，`MISSION_CURRENT` 变为目标序号。
- 测试过程中 SITL 在切换后出现自动解锁/任务状态变化，尚不能证明持续飞行中的完整替换行为。
- 尚未通过 QGC 图形界面或 MagLab 面板执行上传；这仍是协议级探测，不是功能验收。
- 未验证复杂 Survey／Corridor／LLM／Sketch 边界。

## 部分任务更新（新增，开发中）

在原有全量替换之外新增 `updateMode = 1`：

- 使用 MAVLink `MISSION_WRITE_PARTIAL_LIST` + `MISSION_ITEM_INT`，不再调用全量 `writeMissionItems()`。
- 保留 Home 和 current 之前的已执行前缀，不修改已执行任务项。
- 从 current 之后计算第一处变化；替换范围必须连续。
- 由于 ArduPilot 的一次 partial-list 事务不能跨越当前任务总数，新增任务项采用“每项一次 partial-list 事务”逐项追加。
- 不支持通过部分写入减少任务项；任务数量减少必须改用全量替换。
- 不发送 `DO_SET_MISSION_CURRENT`，飞控保持当前执行序号。
- 失败时停止后续追加；已写入的任务项不会自动回滚。
- 首版仅支持 ArduPilot；仍需 SITL/台架验证，不得以协议测试替代真实飞行验收。

代码入口：

- `PlanManager::writePartialMissionItems()`
- `InFlightMissionReplaceController::_buildPartialSplice()`
- `InFlightMissionReplaceController::_startPartialUpload()`
