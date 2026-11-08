#include "MinecraftControls.h"

#include <QMap>
#include <QRegularExpression>

namespace strata::settings
{
    namespace
    {
        QString title(const QString& value)
        {
            QString result;
            bool capitalize = true;
            QChar previous = QLatin1Char(' ');
            for (const QChar character : value)
            {
                if (character == QLatin1Char('.') || character == QLatin1Char('_') ||
                    character.isSpace())
                {
                    if (!result.isEmpty() && !result.endsWith(QLatin1Char(' ')))
                    {
                        result.append(QLatin1Char(' '));
                    }
                    capitalize = true;
                    previous = QLatin1Char(' ');
                    continue;
                }
                if (character.isUpper() && previous.isLower())
                {
                    result.append(QLatin1Char(' '));
                    capitalize = true;
                }
                result.append(capitalize ? character.toUpper() : character);
                capitalize = false;
                previous = character;
            }
            return result.trimmed();
        }
    } // namespace

    QMap<QString, QString> MinecraftControls::defaultBindings()
    {
        return {
            {QStringLiteral("key_key.attack"), QStringLiteral("key.mouse.left")},
            {QStringLiteral("key_key.use"), QStringLiteral("key.mouse.right")},
            {QStringLiteral("key_key.forward"), QStringLiteral("key.keyboard.w")},
            {QStringLiteral("key_key.left"), QStringLiteral("key.keyboard.a")},
            {QStringLiteral("key_key.back"), QStringLiteral("key.keyboard.s")},
            {QStringLiteral("key_key.right"), QStringLiteral("key.keyboard.d")},
            {QStringLiteral("key_key.jump"), QStringLiteral("key.keyboard.space")},
            {QStringLiteral("key_key.sneak"), QStringLiteral("key.keyboard.left.shift")},
            {QStringLiteral("key_key.sprint"), QStringLiteral("key.keyboard.left.control")},
            {QStringLiteral("key_key.inventory"), QStringLiteral("key.keyboard.e")},
            {QStringLiteral("key_key.drop"), QStringLiteral("key.keyboard.q")},
            {QStringLiteral("key_key.swapOffhand"), QStringLiteral("key.keyboard.f")},
            {QStringLiteral("key_key.pickItem"), QStringLiteral("key.mouse.middle")},
            {QStringLiteral("key_key.chat"), QStringLiteral("key.keyboard.t")},
            {QStringLiteral("key_key.playerlist"), QStringLiteral("key.keyboard.tab")},
            {QStringLiteral("key_key.command"), QStringLiteral("key.keyboard.slash")},
            {QStringLiteral("key_key.screenshot"), QStringLiteral("key.keyboard.f2")},
            {QStringLiteral("key_key.togglePerspective"), QStringLiteral("key.keyboard.f5")},
            {QStringLiteral("key_key.smoothCamera"), QStringLiteral("key.keyboard.unknown")},
            {QStringLiteral("key_key.fullscreen"), QStringLiteral("key.keyboard.f11")},
            {QStringLiteral("key_key.hotbar.1"), QStringLiteral("key.keyboard.1")},
            {QStringLiteral("key_key.hotbar.2"), QStringLiteral("key.keyboard.2")},
            {QStringLiteral("key_key.hotbar.3"), QStringLiteral("key.keyboard.3")},
            {QStringLiteral("key_key.hotbar.4"), QStringLiteral("key.keyboard.4")},
            {QStringLiteral("key_key.hotbar.5"), QStringLiteral("key.keyboard.5")},
            {QStringLiteral("key_key.hotbar.6"), QStringLiteral("key.keyboard.6")},
            {QStringLiteral("key_key.hotbar.7"), QStringLiteral("key.keyboard.7")},
            {QStringLiteral("key_key.hotbar.8"), QStringLiteral("key.keyboard.8")},
            {QStringLiteral("key_key.hotbar.9"), QStringLiteral("key.keyboard.9")},
            {QStringLiteral("key_key.saveToolbarActivator"), QStringLiteral("key.keyboard.c")},
            {QStringLiteral("key_key.loadToolbarActivator"), QStringLiteral("key.keyboard.x")},
            {QStringLiteral("key_key.advancements"), QStringLiteral("key.keyboard.l")},
            {QStringLiteral("key_key.friends"), QStringLiteral("key.keyboard.o")},
            {QStringLiteral("key_key.socialInteractions"), QStringLiteral("key.keyboard.p")},
            {QStringLiteral("key_key.toggleGui"), QStringLiteral("key.keyboard.f1")},
            {QStringLiteral("key_key.toggleSpectatorShaderEffects"),
             QStringLiteral("key.keyboard.f4")},
            {QStringLiteral("key_key.spectatorHotbar"), QStringLiteral("key.mouse.middle")},
            {QStringLiteral("key_key.quickActions"), QStringLiteral("key.keyboard.g")},
            {QStringLiteral("key_key.debug.overlay"), QStringLiteral("key.keyboard.f3")},
            {QStringLiteral("key_key.debug.modifier"), QStringLiteral("key.keyboard.f3")},
            {QStringLiteral("key_key.debug.reloadChunk"), QStringLiteral("key.keyboard.a")},
            {QStringLiteral("key_key.debug.showHitboxes"), QStringLiteral("key.keyboard.b")},
            {QStringLiteral("key_key.debug.clearChat"), QStringLiteral("key.keyboard.d")},
            {QStringLiteral("key_key.debug.crash"), QStringLiteral("key.keyboard.c")},
            {QStringLiteral("key_key.debug.showChunkBorders"), QStringLiteral("key.keyboard.g")},
            {QStringLiteral("key_key.debug.showAdvancedTooltips"),
             QStringLiteral("key.keyboard.h")},
            {QStringLiteral("key_key.debug.copyRecreateCommand"), QStringLiteral("key.keyboard.i")},
            {QStringLiteral("key_key.debug.spectate"), QStringLiteral("key.keyboard.n")},
            {QStringLiteral("key_key.debug.switchGameMode"), QStringLiteral("key.keyboard.f4")},
            {QStringLiteral("key_key.debug.debugOptions"), QStringLiteral("key.keyboard.f6")},
            {QStringLiteral("key_key.debug.focusPause"), QStringLiteral("key.keyboard.p")},
            {QStringLiteral("key_key.debug.dumpDynamicTextures"), QStringLiteral("key.keyboard.s")},
            {QStringLiteral("key_key.debug.reloadResourcePacks"), QStringLiteral("key.keyboard.t")},
            {QStringLiteral("key_key.debug.profiling"), QStringLiteral("key.keyboard.l")},
            {QStringLiteral("key_key.debug.copyLocation"), QStringLiteral("key.keyboard.c")},
            {QStringLiteral("key_key.debug.dumpVersion"), QStringLiteral("key.keyboard.v")},
            {QStringLiteral("key_key.debug.profilingChart"), QStringLiteral("key.keyboard.1")},
            {QStringLiteral("key_key.debug.fpsCharts"), QStringLiteral("key.keyboard.2")},
            {QStringLiteral("key_key.debug.networkCharts"), QStringLiteral("key.keyboard.3")},
            {QStringLiteral("key_key.debug.lightmapTexture"), QStringLiteral("key.keyboard.4")},
            {QStringLiteral("key_key.debug.improvedTransparency"),
             QStringLiteral("key.keyboard.x")}};
    }

    QMap<QString, QString> MinecraftControls::bindings(const QMap<QString, QString>& options)
    {
        QMap<QString, QString> result = defaultBindings();
        for (auto option = options.cbegin(); option != options.cend(); ++option)
        {
            if (isBindingKey(option.key()))
            {
                result.insert(option.key(), option.value());
            }
        }
        return result;
    }

    QMap<QString, QStringList> MinecraftControls::conflicts(const QMap<QString, QString>& bindings)
    {
        QMap<QString, QStringList> assigned;
        for (auto binding = bindings.cbegin(); binding != bindings.cend(); ++binding)
        {
            if (binding.value().isNull() ||
                binding.value() == QStringLiteral("key.keyboard.unknown") ||
                binding.key().startsWith(QStringLiteral("key_key.debug.")) ||
                binding.key() == QStringLiteral("key_key.spectatorHotbar"))
            {
                continue;
            }
            assigned[binding.value()].append(binding.key());
        }
        QMap<QString, QStringList> result;
        for (auto assignment = assigned.cbegin(); assignment != assigned.cend(); ++assignment)
        {
            if (assignment.value().size() > 1)
            {
                QStringList keys = assignment.value();
                keys.sort();
                result.insert(assignment.key(), keys);
            }
        }
        return result;
    }

    QString MinecraftControls::actionName(const QString& optionKey)
    {
        if (!isBindingKey(optionKey))
        {
            return optionKey;
        }
        const QString name = optionKey.mid(QStringLiteral("key_key.").size());
        static const QMap<QString, QString> names{
            {QStringLiteral("attack"), QStringLiteral("Attack")},
            {QStringLiteral("use"), QStringLiteral("Use Item / Place Block")},
            {QStringLiteral("forward"), QStringLiteral("Move Forward")},
            {QStringLiteral("left"), QStringLiteral("Strafe Left")},
            {QStringLiteral("back"), QStringLiteral("Move Backward")},
            {QStringLiteral("right"), QStringLiteral("Strafe Right")},
            {QStringLiteral("jump"), QStringLiteral("Jump")},
            {QStringLiteral("sneak"), QStringLiteral("Sneak")},
            {QStringLiteral("sprint"), QStringLiteral("Sprint")},
            {QStringLiteral("inventory"), QStringLiteral("Open Inventory")},
            {QStringLiteral("drop"), QStringLiteral("Drop Item")},
            {QStringLiteral("swapOffhand"), QStringLiteral("Swap Offhand")},
            {QStringLiteral("pickItem"), QStringLiteral("Pick Block")},
            {QStringLiteral("chat"), QStringLiteral("Open Chat")},
            {QStringLiteral("playerlist"), QStringLiteral("Player List")},
            {QStringLiteral("command"), QStringLiteral("Open Command")},
            {QStringLiteral("friends"), QStringLiteral("Friends")},
            {QStringLiteral("socialInteractions"), QStringLiteral("Social Interactions")},
            {QStringLiteral("toggleGui"), QStringLiteral("Toggle HUD")},
            {QStringLiteral("toggleSpectatorShaderEffects"),
             QStringLiteral("Toggle Spectator Effects")},
            {QStringLiteral("screenshot"), QStringLiteral("Take Screenshot")},
            {QStringLiteral("togglePerspective"), QStringLiteral("Toggle Perspective")},
            {QStringLiteral("smoothCamera"), QStringLiteral("Toggle Cinematic Camera")},
            {QStringLiteral("fullscreen"), QStringLiteral("Toggle Fullscreen")},
            {QStringLiteral("spectatorOutlines"), QStringLiteral("Spectator Outlines")},
            {QStringLiteral("spectatorHotbar"), QStringLiteral("Spectator Hotbar")},
            {QStringLiteral("quickActions"), QStringLiteral("Quick Actions")},
            {QStringLiteral("debug.overlay"), QStringLiteral("Debug Overlay")},
            {QStringLiteral("debug.modifier"), QStringLiteral("Debug Modifier")},
            {QStringLiteral("saveToolbarActivator"), QStringLiteral("Save Toolbar Activator")},
            {QStringLiteral("loadToolbarActivator"), QStringLiteral("Load Toolbar Activator")},
            {QStringLiteral("advancements"), QStringLiteral("Advancements")}};
        const auto known = names.constFind(name);
        if (known != names.cend())
        {
            return known.value();
        }
        static const QRegularExpression hotbar(QStringLiteral("^hotbar\\.(.+)$"));
        const auto match = hotbar.match(name);
        return match.hasMatch() ? QStringLiteral("Select Hotbar Slot ") + match.captured(1)
                                : title(name);
    }

    QString MinecraftControls::displayKey(const QString& value)
    {
        if (value.isNull() || value == QStringLiteral("key.keyboard.unknown"))
        {
            return QStringLiteral("Unassigned");
        }
        if (value.startsWith(QStringLiteral("key.mouse.")))
        {
            return QStringLiteral("Mouse ") + title(value.mid(QStringLiteral("key.mouse.").size()));
        }
        if (value.startsWith(QStringLiteral("key.keyboard.")))
        {
            return title(value.mid(QStringLiteral("key.keyboard.").size()));
        }
        return value;
    }

    bool MinecraftControls::isBindingKey(const QString& key)
    {
        return key.startsWith(QStringLiteral("key_key."));
    }
} // namespace strata::settings
