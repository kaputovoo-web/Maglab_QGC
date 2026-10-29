# 高程基准证据与启用门槛

## 已确认的事实

### Copernicus 源产品

官方产品说明：
https://dataspace.copernicus.eu/explore-data/data-collections/copernicus-contributing-missions/collections-description/COP-DEM

本地抓取存于构建目录 `copernicus-dem-official.html`。官方页面明确列出：

- Vertical EGM2008 (EPSG 3855)，单位米。
- 产品属于 DSM，包含建筑、基础设施和植被表面的影响，不是裸地 DTM。

这是 **Copernicus 产品规范**，并不能证明第三方服务输出及本地缓存仍使用该基准。

### 当前应用数据链路

- `ElevationMapProvider.h` 标识提供方为 Copernicus，端点为 `terrain-ce.suite.auterion.com/api/v1/carpet`。
- 黄岛探测响应含 status、bounds、stats、carpet，没有 datum、产品版本或转换历史字段。
- `TerrainTileCopernicus::serializeFromData()` 将高程写为 int16；缓存没有垂直基准和产品版本元数据。
- 尚无服务端关于“原样保持 EGM2008”的已验证契约。因此运行状态仍应视为未知基准，不能直接赋予 EPSG:3855。

### 载具与任务

- `VehicleFactGroup` 接收 MAVLink ALTITUDE.altitude_amsl，原始值以米存储。字段称 AMSL 不足以确认具体采用哪个 geoid；需要对应固件版本/高度估计来源依据。
- `SimpleMissionItem::amslEntryAlt()` 按高度框架处理：相对高度加 plannedHome 海拔；terrain 高度加已查询地形；绝对高度直接返回任务参数。
- 任务的 terrain 高度仍依赖地形数据链路，不能仅因属性名包含 AMSL 就认为跨基准已经正确。

## 禁止的捷径

- 不使用某一固定差值将所有 DEM 高度转换成飞控 AMSL。
- 不自动将 Home 高度与附近 DSM 样本相减，冒充严格基准转换。
- 不把 EGM96、EGM2008 和 WGS84 椭球高当作同一高度。
- 不将 DSM 直接称为裸地；在 DSM 建筑表面再挤出 OSM 建筑可能重复计算高度。

## 数学关系与实现前提

椭球高 h 与正高 H 的关系为 H = h - N。若源为 EGM2008 正高、目标为 EGM96 正高，则 H96 = H2008 + N2008 - N96。每个 N 必须按该点位置从已知版本、许可和精度的 geoid 模型求取，不能使用整个区域的猜测常量。还必须确认飞控实际目标基准，不能默认是 EGM96。

## 完整交付前仍需

1. 服务提供方或可复现源数据证明实际输出的产品和基准；缓存记录相应版本，防止混用。
2. 固件/估计器高度来源依据；若无法确定则维持未知状态。
3. 如基准不同，引入有校验和、许可证与版本记录的 geoid 数据，并对公开控制值测试转换方向、米制、边界和缺值。
4. 建筑使用可信裸地 DTM 或明确标识的近似基础策略，不能宣称 DSM 上的建筑高度为真实总高度。
5. 缺失、断链、场景参考改变、任务编辑、超时与迟到回调测试通过；实际 Qt 材质与设备显示验证。

在上述门槛完成前，仅允许明确标识的局部地形预览，不启用宣称已统一海拔的载具/任务叠加，不输出净空安全结论。
