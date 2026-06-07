#include "LocalDictTranslator.h"
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QStandardPaths>
#include <QtCore/QFileInfo>
#include <QtNetwork/QNetworkRequest>

// --- Default built-in EN→ZH dictionary (~100 common UI / software terms) ---
static const char* kDefaultDict[][2] = {
    {"file", "文件"}, {"edit", "编辑"}, {"view", "查看"}, {"help", "帮助"},
    {"open", "打开"}, {"save", "保存"}, {"close", "关闭"}, {"exit", "退出"},
    {"new", "新建"}, {"delete", "删除"}, {"copy", "复制"}, {"cut", "剪切"},
    {"paste", "粘贴"}, {"undo", "撤销"}, {"redo", "重做"}, {"select", "选择"},
    {"select all", "全选"}, {"find", "查找"}, {"replace", "替换"},
    {"settings", "设置"}, {"options", "选项"}, {"preferences", "首选项"},
    {"tools", "工具"}, {"window", "窗口"}, {"format", "格式"}, {"insert", "插入"},
    {"table", "表格"}, {"image", "图片"}, {"link", "链接"}, {"page", "页面"},
    {"print", "打印"}, {"preview", "预览"}, {"export", "导出"}, {"import", "导入"},
    {"search", "搜索"}, {"refresh", "刷新"}, {"reload", "重新加载"},
    {"back", "返回"}, {"forward", "前进"}, {"home", "主页"}, {"next", "下一步"},
    {"previous", "上一步"}, {"finish", "完成"}, {"cancel", "取消"}, {"ok", "确定"},
    {"yes", "是"}, {"no", "否"}, {"apply", "应用"}, {"reset", "重置"},
    {"add", "添加"}, {"remove", "移除"}, {"edit item", "编辑项目"},
    {"rename", "重命名"}, {"properties", "属性"}, {"details", "详细信息"},
    {"list", "列表"}, {"grid", "网格"}, {"thumbnail", "缩略图"},
    {"sort", "排序"}, {"filter", "筛选"}, {"group", "分组"},
    {"ascending", "升序"}, {"descending", "降序"},
    {"login", "登录"}, {"logout", "登出"}, {"register", "注册"},
    {"username", "用户名"}, {"password", "密码"}, {"email", "邮箱"},
    {"submit", "提交"}, {"confirm", "确认"}, {"verify", "验证"},
    {"download", "下载"}, {"upload", "上传"}, {"install", "安装"},
    {"uninstall", "卸载"}, {"update", "更新"}, {"upgrade", "升级"},
    {"version", "版本"}, {"about", "关于"}, {"license", "许可证"},
    {"error", "错误"}, {"warning", "警告"}, {"info", "信息"},
    {"success", "成功"}, {"failed", "失败"}, {"retry", "重试"}, {"skip", "跳过"},
    {"loading", "加载中"}, {"please wait", "请稍候"},
    {"enable", "启用"}, {"disable", "禁用"}, {"on", "开"}, {"off", "关"},
    {"default", "默认"}, {"custom", "自定义"}, {"advanced", "高级"},
    {"language", "语言"}, {"font", "字体"}, {"color", "颜色"},
    {"size", "大小"}, {"small", "小"}, {"medium", "中"}, {"large", "大"},
    {"full screen", "全屏"}, {"minimize", "最小化"}, {"maximize", "最大化"},
    {"restore", "还原"}, {"zoom in", "放大"}, {"zoom out", "缩小"},
};

LocalDictTranslator::LocalDictTranslator(QObject* parent)
    : ITranslator(parent), m_nam(new QNetworkAccessManager(this))
{
    QString appDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appDir);
    m_dictPath = appDir + "/local_dict.json";
    ensureDefaultDict();
    loadDictionary();
}

// --- Config ---
QVector<TranslatorConfigField> LocalDictTranslator::configFields() const {
    return {
        {"dictPath",    "Dictionary Path", m_dictPath,    false, false},
        {"downloadUrl", "Download URL",    "",             false, false},
    };
}

void LocalDictTranslator::setConfig(const QString& key, const QString& value) {
    if (key == "dictPath")    m_dictPath    = value;
    if (key == "downloadUrl") m_downloadUrl = value;
}

QString LocalDictTranslator::getConfig(const QString& key) const {
    if (key == "dictPath")    return m_dictPath;
    if (key == "downloadUrl") return m_downloadUrl;
    return {};
}

// --- Dictionary I/O ---
void LocalDictTranslator::ensureDefaultDict() {
    if (QFile::exists(m_dictPath)) return;

    QJsonObject entries;
    for (const auto& pair : kDefaultDict)
        entries.insert(QString::fromUtf8(pair[0]), QString::fromUtf8(pair[1]));

    QJsonObject root;
    root["source"] = QStringLiteral("en");
    root["target"] = QStringLiteral("zh");
    root["entries"] = entries;

    QFile f(m_dictPath);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    }
}

void LocalDictTranslator::loadDictionary() {
    m_dict.clear();
    QFile f(m_dictPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    QJsonObject entries = doc.object().value("entries").toObject();
    for (auto it = entries.begin(); it != entries.end(); ++it)
        m_dict.insert(it.key().toLower(), it.value().toString());
}

// --- Download ---
void LocalDictTranslator::downloadDictionary() {
    if (m_downloadUrl.isEmpty()) return;

    QNetworkRequest request{QUrl(m_downloadUrl)};
    QNetworkReply* reply = m_nam->get(request);
    m_pendingRequests.append(reply);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        m_pendingRequests.removeOne(reply);
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit translationError(tr("Dictionary download failed: ") + reply->errorString());
            return;
        }

        QByteArray data = reply->readAll();
        // Validate JSON
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isObject() || !doc.object().contains("entries")) {
            emit translationError(tr("Invalid dictionary format"));
            return;
        }

        QFile f(m_dictPath);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            f.write(data);
            f.close();
            loadDictionary();
            emit translationReady(QString(), QString()); // signal: dict reloaded
        }
    });
}

// --- Translate ---
QString LocalDictTranslator::lookupBest(const QString& text) const {
    QString t = text.trimmed();
    if (t.isEmpty()) return {};

    // 1. Exact match (lowercase)
    auto it = m_dict.constFind(t.toLower());
    if (it != m_dict.constEnd()) return it.value();

    // 2. Case-insensitive scan
    for (auto i = m_dict.begin(); i != m_dict.end(); ++i) {
        if (i.key().compare(t, Qt::CaseInsensitive) == 0)
            return i.value();
    }

    // 3. Word-by-word: split, translate each, rejoin
    QStringList parts = t.split(' ', Qt::SkipEmptyParts);
    if (parts.size() > 1) {
        QStringList translated;
        for (const QString& part : parts) {
            auto jt = m_dict.constFind(part.toLower());
            translated.append(jt != m_dict.constEnd() ? jt.value() : part);
        }
        // Only use word-by-word if at least one word was translated
        if (translated != parts)
            return translated.join(' ');
    }

    return {};  // no match
}

void LocalDictTranslator::translate(const TranslateRequest& req) {
    QString result = lookupBest(req.text);
    if (!result.isEmpty()) {
        emit translationReady(req.text, result);
    } else {
        // No match — emit the original as-is for offline fallback display
        emit translationReady(req.text, req.text);
    }
}

void LocalDictTranslator::cancelAll() {
    for (auto* reply : m_pendingRequests) {
        reply->abort();
        reply->deleteLater();
    }
    m_pendingRequests.clear();
}
