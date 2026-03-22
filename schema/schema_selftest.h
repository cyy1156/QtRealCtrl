// schema_selftest.h — 仅用于本地验证 schema + TLV 映射（可随时从 main 去掉）
#pragma once

class QString;

/// @return 全部通过为 true；失败时若有 report 则写入明细
bool runSchemaSelfTest(QString* report = nullptr);
