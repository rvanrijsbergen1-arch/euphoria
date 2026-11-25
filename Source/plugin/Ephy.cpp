#include "Ephy.h"

namespace
{
// Ephy's body in a 400 x 520 view box. Original EPH SAMPLES character: teal hat with a lightning band,
// violet robe with gold sash and stars, short white beard in two braids with gold beads, DJ headphones, wooden staff.
const char* kBodySvg = R"EPHY(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 400 520" width="400" height="520">
<defs><linearGradient id="robe" gradientUnits="userSpaceOnUse" x1="0" y1="290" x2="0" y2="510"><stop offset="0" stop-color="#3A2C7A"/><stop offset="1" stop-color="#1B1440"/></linearGradient></defs>
<g stroke="#0E0A1F" stroke-width="3" stroke-linejoin="round" stroke-linecap="round">
<path d="M130 500 Q118 385 168 300 L242 300 Q290 385 278 500 Q204 516 130 500 Z" fill="url(#robe)"/>
<path d="M140 500 Q118 494 120 512 L170 512 Q176 500 160 498 Z" fill="#2E2266"/>
<path d="M268 500 Q292 492 296 506 Q290 516 248 512 Q244 502 268 500 Z" fill="#2E2266"/>
<path d="M162 360 Q205 376 248 360 L250 380 Q205 396 160 380 Z" fill="#C9A646"/>
<path d="M170 305 Q118 340 114 398 Q138 410 158 394 Q160 352 184 322 Z" fill="#4A33B0"/>
<circle cx="124" cy="402" r="12" fill="#F2C9A0"/>
<line x1="306" y1="505" x2="306" y2="172" stroke="#6B4A2E" stroke-width="10"/>
<path d="M306 176 Q284 160 292 136 M306 176 Q328 160 320 136" stroke="#6B4A2E" stroke-width="6" fill="none"/>
<path d="M168 300 Q205 334 242 300" fill="none" stroke="#1FB5A8" stroke-width="7"/>
<ellipse cx="166" cy="296" rx="11" ry="14" fill="#1FB5A8"/>
<ellipse cx="244" cy="296" rx="11" ry="14" fill="#1FB5A8"/>
<circle cx="205" cy="246" r="42" fill="#F2C9A0"/>
<path d="M166 254 Q168 302 190 308 Q205 302 220 308 Q242 302 244 254 Q205 278 166 254 Z" fill="#F4F1FA"/>
<path d="M190 304 Q183 314 190 322 Q183 332 190 340 Q183 350 190 356 M220 304 Q227 314 220 322 Q227 332 220 340 Q227 350 220 356" fill="none" stroke="#F4F1FA" stroke-width="11"/>
</g>
<circle cx="190" cy="362" r="5" fill="#C9A646" stroke="#0E0A1F" stroke-width="2"/>
<circle cx="220" cy="362" r="5" fill="#C9A646" stroke="#0E0A1F" stroke-width="2"/>
<g stroke="#0E0A1F" stroke-width="3" stroke-linejoin="round" stroke-linecap="round">
<path d="M178 260 Q205 244 232 260 Q216 272 205 263 Q194 272 178 260 Z" fill="#FFFFFF"/>
<ellipse cx="205" cy="250" rx="10" ry="8" fill="#E8A882"/>
<ellipse cx="190" cy="232" rx="8" ry="10" fill="#FFFFFF"/>
<ellipse cx="220" cy="232" rx="8" ry="10" fill="#FFFFFF"/>
</g>
<path d="M176 220 Q190 208 200 220 M210 220 Q220 208 234 220" fill="none" stroke="#F4F1FA" stroke-width="7" stroke-linecap="round"/>
<g stroke="#0E0A1F" stroke-width="3" stroke-linejoin="round" stroke-linecap="round">
<path d="M158 210 Q168 142 192 94 Q204 66 212 40 Q224 92 238 142 Q250 182 252 210 Z" fill="#1F6F8B"/>
<ellipse cx="205" cy="212" rx="76" ry="15" fill="#1A5C74"/>
<path d="M162 196 Q205 206 250 196 L252 208 Q205 218 160 208 Z" fill="#1FB5A8"/>
<path d="M212 26 L216 34 L225 35 L218 41 L220 50 L212 45 L204 50 L206 41 L199 35 L208 34 Z" fill="#C9A646"/>
<path d="M236 306 Q276 302 300 284 L312 304 Q282 332 246 336 Z" fill="#4A33B0"/>
<circle cx="306" cy="296" r="12" fill="#F2C9A0"/>
</g>
<path d="M202 198 L196 208 L202 208 L198 217 L208 205 L202 205 L206 198 Z" fill="#FFE9A8"/>
<path d="M150 440 L153 446 L160 447 L155 452 L156 459 L150 456 L144 459 L145 452 L140 447 L147 446 Z M250 430 L253 436 L260 437 L255 442 L256 449 L250 446 L244 449 L245 442 L240 437 L247 436 Z M200 470 L202 474 L207 475 L203 478 L204 483 L200 481 L196 483 L197 478 L193 475 L198 474 Z" fill="#C9A646"/>
</svg>)EPHY";

constexpr float kOrbX = 306.0f, kOrbY = 140.0f;
}

EphyView::EphyView()
{
    setInterceptsMouseClicks (false, false);
    rebuildBody();
}

void EphyView::rebuildBody()
{
    if (auto xml = juce::XmlDocument::parse (kBodySvg))
        body = juce::Drawable::createFromSVG (*xml);
}

juce::String EphyView::rankFor (float k, bool asleep)
{
    if (asleep || k <= 0.0005f) return "RESTING";
    if (k >= 0.995f) return "GRAND ARCHMAGE";
    if (k >= 0.7f) return "ARCHMAGE";
    if (k >= 0.3f) return "SORCERER";
    return "APPRENTICE";
}

juce::String EphyView::spellFor (float k, bool asleep)
{
    if (asleep || k <= 0.0005f) return "Ephy is taking a nap";
    if (k >= 0.7f) return "casting: Festival Ascension";
    if (k >= 0.3f) return "casting: Hall of Echoes";
    return "casting: Wide Charm";
}

void EphyView::setState (float p, juce::Colour a, bool asleep, float audioLevel, double timeSec)
{
    power = juce::jlimit (0.0f, 1.0f, p);
    accent = a;
    sleeping = asleep;
    level = juce::jlimit (0.0f, 1.0f, audioLevel);
    t = timeSec;
    repaint();
}

void EphyView::paint (juce::Graphics& g)
{
    const float k = sleeping ? 0.0f : power;
    const float sc = juce::jmin (1.0f, juce::jmin (getWidth() / 400.0f, getHeight() / 520.0f));   // natural size, room around it for the aura
    const float ox = (getWidth() - 400.0f * sc) * 0.5f, oy = (getHeight() - 520.0f * sc) * 0.5f;
    const auto view = juce::AffineTransform::scale (sc).translated (ox, oy);
    const juce::Colour gold (0xffc9a646);

    // aura
    {
        const float r = 90.0f + k * 150.0f;
        juce::ColourGradient grad (accent.withAlpha (0.55f * (0.15f + 0.85f * k)), juce::Point<float> (210, 300).transformedBy (view),
                                   accent.withAlpha (0.0f), juce::Point<float> (210 + r, 300).transformedBy (view), true);
        g.setGradientFill (grad);
        g.fillEllipse (juce::Rectangle<float> (210 - r, 300 - r, 2 * r, 2 * r).transformedBy (view));
    }

    // magic circle at his feet
    if (k > 0.01f)
    {
        g.setColour (accent.withAlpha (0.9f * k));
        for (int ring = 0; ring < 2; ++ring)
        {
            const float rx = ring == 0 ? 50 + k * 110 : 36 + k * 90, ry = ring == 0 ? 10 + k * 22 : 7 + k * 17;
            juce::Path e;
            e.addEllipse (200 - rx, 500 - ry, 2 * rx, 2 * ry);
            if (ring == 0)
            {
                // dashed ring whose dashes travel around (a circle rotated, then squashed into the ellipse)
                juce::Path circle, dashed;
                circle.addEllipse (-rx, -rx, 2 * rx, 2 * rx);
                const float dashes[] = { 6.0f, 8.0f };
                juce::PathStrokeType (2.0f).createDashedStroke (dashed, circle, dashes, 2);
                const auto rot = juce::AffineTransform::rotation ((float) std::fmod (t * (0.15 + k * 0.6), juce::MathConstants<double>::twoPi))
                                     .scaled (1.0f, ry / juce::jmax (1.0f, rx)).translated (200, 500).followedBy (view);
                g.fillPath (dashed, rot);
            }
            else
                g.strokePath (e, juce::PathStrokeType (1.0f), view);
        }
    }

    // gentle float, faster with more power
    const float period = 4.0f - k * 2.0f;
    const float bob = -3.0f * (float) std::sin (t * juce::MathConstants<double>::twoPi / period);
    const auto bodyT = juce::AffineTransform::translation (0, bob).followedBy (view);

    if (body) body->draw (g, 1.0f, bodyT);

    // eyes: glow from ARCHMAGE (70 %) on
    const bool eyesLit = k >= 0.7f;
    for (float ex : { 190.0f, 220.0f })
    {
        if (eyesLit)
        {
            g.setColour (accent.withAlpha (juce::jmin (1.0f, 0.5f + (k - 0.7f) * 1.6f)));
            g.fillEllipse (juce::Rectangle<float> (ex - 10, 222, 20, 20).transformedBy (bodyT));
        }
        g.setColour (eyesLit ? juce::Colours::white : juce::Colour (0xff1a1430));
        g.fillEllipse (juce::Rectangle<float> (ex + 1 - 4, 234 - 4, 8, 8).transformedBy (bodyT));
    }

    // light rays from the orb
    const juce::Point<float> orb (kOrbX, kOrbY + bob);
    const int rays = juce::roundToInt (k * 14.0f);
    if (rays > 0)
    {
        juce::Path p;
        const float spin = (float) (t * 0.25);
        for (int i = 0; i < rays; ++i)
        {
            const float a = (float) i * juce::MathConstants<float>::twoPi / (float) rays + 0.3f + spin;
            const float len = 24.0f + k * 70.0f * (0.6f + (float) (i % 3) * 0.2f) * (1.0f + 0.25f * level);
            p.startNewSubPath (orb.x + std::cos (a) * 20.0f, orb.y + std::sin (a) * 20.0f);
            p.lineTo (orb.x + std::cos (a) * len, orb.y + std::sin (a) * len);
        }
        g.setColour (accent.withAlpha (0.3f + 0.6f * k));
        g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), view);
    }

    // orb glow + pulsing core (also reacts to the audio)
    {
        const float r = (14.0f + k * 70.0f) * (1.0f + 0.15f * level);
        const auto c = orb.transformedBy (view);
        juce::ColourGradient grad (juce::Colours::white.withAlpha (0.25f + 0.75f * k), c, accent.withAlpha (0.0f), c.translated (r * sc, 0), true);
        grad.addColour (0.35, accent.withAlpha (0.25f + 0.75f * k));
        g.setGradientFill (grad);
        g.fillEllipse (juce::Rectangle<float> (orb.x - r, orb.y - r, 2 * r, 2 * r).transformedBy (view));
        const float pulse = 1.0f + 0.08f * (float) std::sin (t * juce::MathConstants<double>::twoPi / 1.6) + 0.35f * level * k;
        const float cr = (6.0f + k * 8.0f) * pulse;
        g.setColour (juce::Colours::white.withAlpha (0.5f + 0.5f * k));
        g.fillEllipse (juce::Rectangle<float> (orb.x - cr, orb.y - cr, 2 * cr, 2 * cr).transformedBy (view));
    }

    // sparks
    const int n = juce::roundToInt (k * 26.0f);
    for (int i = 0; i < n; ++i)
    {
        const float ang = (float) i * 2.399f + (float) (t * 0.15);
        const float rad = 30.0f + (float) ((i * 37) % 100) * (0.4f + k * 1.1f);
        const float x = orb.x + std::cos (ang) * rad, y = orb.y + std::sin (ang) * rad * 0.9f;
        const float tw = 0.35f + 0.65f * (0.5f + 0.5f * (float) std::sin (t * (3.0 + (i % 5)) + i));
        const bool white = i % 3 == 0;
        const float r = white ? 1.6f + k * 1.2f : 1.8f + k * 2.0f;
        g.setColour ((white ? juce::Colours::white : accent).withAlpha (tw));
        g.fillEllipse (juce::Rectangle<float> (x - r, y - r, 2 * r, 2 * r).transformedBy (view));
    }
    juce::ignoreUnused (gold);
}
