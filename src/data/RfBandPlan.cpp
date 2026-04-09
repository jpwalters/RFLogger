#include "RfBandPlan.h"

QVector<RfBand> RfBandPlan::bandsInRange(double startHz, double stopHz) const
{
    QVector<RfBand> result;
    result.reserve(m_bands.size());
    for (const auto& band : m_bands) {
        if (band.stopFreqHz > startHz && band.startFreqHz < stopHz)
            result.append(band);
    }
    return result;
}

QStringList RfBandPlan::availableRegions()
{
    return { "US", "Europe" };
}

RfBandPlan RfBandPlan::forRegion(const QString& region)
{
    if (region == "Europe")
        return eu();
    return us();   // default
}

RfBandPlan RfBandPlan::us()
{
    RfBandPlan plan;
    constexpr double MHz = 1'000'000.0;

    // Colors by category
    QColor lteColor(255, 80, 80);          // Red — cellular LTE/5G
    QColor publicSafetyColor(255, 200, 0); // Gold — public safety
    QColor part74Color(100, 200, 100);     // Green — licensed wireless mics (Part 74)
    QColor ismColor(150, 100, 255);        // Purple — ISM / unlicensed
    QColor aeroColor(80, 180, 255);        // Light blue — aeronautical

    // ── LTE / 5G Cellular ────────────────────────────────────────────────

    plan.m_bands.append({"LTE Band 12/17\n(700 MHz Lower)",
                         699 * MHz, 716 * MHz, "LTE", lteColor});
    plan.m_bands.append({"LTE Band 13\n(700 MHz Upper)",
                         746 * MHz, 756 * MHz, "LTE", lteColor});
    plan.m_bands.append({"LTE Band 14\n(FirstNet)",
                         758 * MHz, 768 * MHz, "LTE", lteColor});
    plan.m_bands.append({"LTE Band 5\n(850 CLR)",
                         824 * MHz, 849 * MHz, "LTE", lteColor});
    plan.m_bands.append({"LTE Band 71\n(600 MHz)",
                         617 * MHz, 652 * MHz, "LTE", lteColor});
    plan.m_bands.append({"LTE Band 71 DL\n(600 MHz DL)",
                         652 * MHz, 698 * MHz, "LTE", lteColor});
    plan.m_bands.append({"LTE Band 2/25\n(1900 PCS)",
                         1850 * MHz, 1995 * MHz, "LTE", lteColor});
    plan.m_bands.append({"LTE Band 4/66\n(AWS)",
                         1710 * MHz, 1780 * MHz, "LTE", lteColor});

    // ── Public Safety ────────────────────────────────────────────────────

    plan.m_bands.append({"Public Safety\nVHF",
                         150 * MHz, 174 * MHz, "Public Safety", publicSafetyColor});
    plan.m_bands.append({"Public Safety\nUHF (T-Band)",
                         470 * MHz, 512 * MHz, "Public Safety", publicSafetyColor});
    plan.m_bands.append({"700 MHz\nPublic Safety",
                         769 * MHz, 775 * MHz, "Public Safety", publicSafetyColor});
    plan.m_bands.append({"700 MHz\nPS Broadband",
                         758 * MHz, 768 * MHz, "Public Safety", publicSafetyColor});

    // ── Part 74 Licensed Wireless Microphones ────────────────────────────

    plan.m_bands.append({"Part 74\nVHF Mics",
                         174 * MHz, 216 * MHz, "Part 74", part74Color});
    plan.m_bands.append({"Part 74\nUHF Mics",
                         470 * MHz, 608 * MHz, "Part 74", part74Color});
    plan.m_bands.append({"Part 74\n941-960 Mics",
                         941 * MHz, 960 * MHz, "Part 74", part74Color});

    // ── ISM / Unlicensed ─────────────────────────────────────────────────

    plan.m_bands.append({"ISM 900 MHz",
                         902 * MHz, 928 * MHz, "ISM", ismColor});
    plan.m_bands.append({"ISM 2.4 GHz",
                         2400 * MHz, 2500 * MHz, "ISM", ismColor});
    plan.m_bands.append({"ISM 5.8 GHz",
                         5725 * MHz, 5875 * MHz, "ISM", ismColor});

    // ── Aeronautical / Radionavigation ───────────────────────────────────

    plan.m_bands.append({"Aero DME",
                         960 * MHz, 1215 * MHz, "Aero", aeroColor});

    return plan;
}

RfBandPlan RfBandPlan::eu()
{
    RfBandPlan plan;
    constexpr double MHz = 1'000'000.0;

    QColor lteColor(255, 80, 80);
    QColor pmseColor(100, 200, 100);       // PMSE (EU equivalent of Part 74)
    QColor ismColor(150, 100, 255);

    // ── LTE / 5G ─────────────────────────────────────────────────────────

    plan.m_bands.append({"LTE Band 20\n(800 DD)",
                         791 * MHz, 862 * MHz, "LTE", lteColor});
    plan.m_bands.append({"LTE Band 28\n(700 DD)",
                         703 * MHz, 758 * MHz, "LTE", lteColor});
    plan.m_bands.append({"LTE Band 32\n(1500 SDL)",
                         1452 * MHz, 1496 * MHz, "LTE", lteColor});

    // ── PMSE (Programme Making and Special Events) ───────────────────────

    plan.m_bands.append({"PMSE\nChannel 38",
                         606 * MHz, 614 * MHz, "PMSE", pmseColor});
    plan.m_bands.append({"PMSE\nUHF",
                         470 * MHz, 694 * MHz, "PMSE", pmseColor});
    plan.m_bands.append({"PMSE\n823-832",
                         823 * MHz, 832 * MHz, "PMSE", pmseColor});
    plan.m_bands.append({"PMSE\n863-865",
                         863 * MHz, 865 * MHz, "PMSE", pmseColor});
    plan.m_bands.append({"PMSE\n1785-1800",
                         1785 * MHz, 1800 * MHz, "PMSE", pmseColor});

    // ── ISM / Unlicensed ─────────────────────────────────────────────────

    plan.m_bands.append({"ISM 868 MHz",
                         863 * MHz, 870 * MHz, "ISM", ismColor});
    plan.m_bands.append({"ISM 2.4 GHz",
                         2400 * MHz, 2500 * MHz, "ISM", ismColor});

    return plan;
}
