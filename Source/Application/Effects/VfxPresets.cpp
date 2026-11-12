#include "pch.h"
#include "VfxPresets.hpp"

#include"Graphics/Particles/ParticleSystem.hpp"
#include"Math/Random/Random.hpp"
#include<cmath>

namespace
{
	using Engine::Graphics::Particle;
	using Engine::Graphics::ParticleShape;
	using Engine::Graphics::Color;

	float Rand(float a, float b)
	{
		return Random::Range(a, b);
	}

	Math::Vector3 RandomUnit()
	{
		const float z = Rand(-1.0f, 1.0f);
		const float t = Rand(0.0f, 6.2831853f);
		const float r = std::sqrt((std::max)(0.0f, 1.0f - z * z));
		return { r * std::cos(t), z, r * std::sin(t) };
	}

	//	Direction を中心に Spread(0..1) の円錐内でランダム
	Math::Vector3 RandomCone(const Math::Vector3& Direction, float Spread)
	{
		Math::Vector3 v = Direction + RandomUnit() * Spread;
		if (v.SqrLength() < 1e-6f)
		{
			return RandomUnit();
		}
		return Math::Vector3::Normalize(v);
	}

	Color Scale(const Color& c, float s, float a)
	{
		return { c.r * s, c.g * s, c.b * s, a };
	}

	//	全体の発光の強さ(ブルームで白飛びしないように調整)
	constexpr float GlobalBrightness = 0.45f;

	void Spawn(const Particle& p)
	{
		Particle scaled = p;
		if (scaled.Additive)
		{
			scaled.ColorStart = Scale(scaled.ColorStart, GlobalBrightness, scaled.ColorStart.a);
			scaled.ColorEnd = Scale(scaled.ColorEnd, GlobalBrightness, scaled.ColorEnd.a);
		}
		Engine::Graphics::ParticleSystem::GetInstance()->Spawn(scaled);
	}

	void Flash(const Math::Vector3& pos, const Color& color, float size, float life, ParticleShape shape = ParticleShape::Flare)
	{
		Particle p;
		p.Position = pos;
		p.Life = life;
		p.SizeStart = size;
		p.SizeEnd = size * 0.4f;
		p.ColorStart = color;
		p.ColorEnd = Scale(color, 0.5f, 0.0f);
		p.Shape = shape;
		p.Rotation = Rand(0.0f, 0.8f);
		Spawn(p);
	}

	void Ring(const Math::Vector3& pos, const Color& color, float sizeStart, float sizeEnd, float life, bool ground)
	{
		Particle p;
		p.Position = pos;
		p.Life = life;
		p.SizeStart = sizeStart;
		p.SizeEnd = sizeEnd;
		p.ColorStart = color;
		p.ColorEnd = Scale(color, 0.3f, 0.0f);
		p.Shape = ground ? ParticleShape::GroundRing : ParticleShape::Ring;
		Spawn(p);
	}

	void Sparks(const Math::Vector3& pos, const Math::Vector3& dir, float spread, int count, const Color& color, float speedMin, float speedMax, float size, float life)
	{
		for (int i = 0; i < count; i++)
		{
			Particle p;
			p.Position = pos;
			p.Velocity = RandomCone(dir, spread) * Rand(speedMin, speedMax);
			p.Life = life * Rand(0.6f, 1.2f);
			p.SizeStart = size * Rand(0.6f, 1.2f);
			p.SizeEnd = 0.0f;
			p.ColorStart = color;
			p.ColorEnd = Scale(color, 0.4f, 0.0f);
			p.Drag = 3.0f;
			p.Gravity = 14.0f;
			p.Stretch = 0.035f;
			p.Shape = ParticleShape::Spark;
			Spawn(p);
		}
	}
}

namespace Engine::Vfx
{
	void HitSpark(const Math::Vector3& Position, const Math::Vector3& Direction, const Graphics::Color& Tint, float ScaleFactor)
	{
		const Math::Vector3 dir = Direction.SqrLength() > 1e-6f ? Math::Vector3::Normalize(Direction) : Math::Vector3(0, 1, 0);
		Flash(Position, Scale(Tint, 14.0f, 1.0f), 2.2f * ScaleFactor, 0.12f);
		Flash(Position, { 6.0f, 6.0f, 6.0f, 1.0f }, 0.9f * ScaleFactor, 0.08f, ParticleShape::SoftDot);
		Ring(Position, Scale(Tint, 5.0f, 0.9f), 0.3f * ScaleFactor, 2.6f * ScaleFactor, 0.22f, false);
		Sparks(Position, dir, 0.9f, static_cast<int>(24 * ScaleFactor), Scale(Tint, 10.0f, 1.0f), 10.0f, 34.0f, 0.35f * ScaleFactor, 0.35f);
		Sparks(Position, dir, 1.6f, 10, { 8.0f, 7.0f, 6.0f, 1.0f }, 4.0f, 14.0f, 0.2f, 0.25f);
	}

	void ParryFlash(const Math::Vector3& Position)
	{
		const Color blue = { 0.55f, 0.8f, 1.0f, 1.0f };
		Flash(Position, Scale(blue, 22.0f, 1.0f), 3.5f, 0.2f);
		Flash(Position, { 20.0f, 20.0f, 20.0f, 1.0f }, 1.8f, 0.12f, ParticleShape::SoftDot);
		Ring(Position, Scale(blue, 10.0f, 1.0f), 0.5f, 6.5f, 0.35f, false);
		Ring(Position, Scale(blue, 6.0f, 0.8f), 0.3f, 4.0f, 0.45f, false);
		Sparks(Position, { 0, 0.3f, 0 }, 1.8f, 48, { 9.0f, 11.0f, 14.0f, 1.0f }, 14.0f, 42.0f, 0.4f, 0.45f);
		Sparks(Position, { 0, 1, 0 }, 1.2f, 16, { 14.0f, 9.0f, 4.0f, 1.0f }, 8.0f, 22.0f, 0.3f, 0.5f);
	}

	void GuardSpark(const Math::Vector3& Position)
	{
		const Color orange = { 1.0f, 0.55f, 0.2f, 1.0f };
		Flash(Position, Scale(orange, 10.0f, 1.0f), 1.6f, 0.1f);
		Sparks(Position, { 0, 0.6f, 0 }, 1.4f, 14, Scale(orange, 9.0f, 1.0f), 6.0f, 20.0f, 0.3f, 0.3f);
	}

	void Telegraph(const Math::Vector3& Position, bool Parryable)
	{
		const Color c = Parryable ? Color{ 14.0f, 14.0f, 16.0f, 1.0f } : Color{ 18.0f, 1.5f, 1.0f, 1.0f };
		Particle p;
		p.Position = Position;
		p.Life = 0.35f;
		p.SizeStart = 0.4f;
		p.SizeEnd = 3.4f;
		p.ColorStart = c;
		p.ColorEnd = Scale(c, 0.2f, 0.0f);
		p.Shape = ParticleShape::Flare;
		p.RotationSpeed = 1.5f;
		Spawn(p);
		Ring(Position, Scale(c, 0.4f, 0.8f), 2.5f, 0.3f, 0.3f, false);
	}

	void DodgeDust(const Math::Vector3& FootPosition)
	{
		for (int i = 0; i < 10; i++)
		{
			Particle p;
			const float angle = Rand(0.0f, 6.2831853f);
			p.Position = FootPosition + Math::Vector3(std::cos(angle), 0.2f, std::sin(angle)) * Rand(0.2f, 1.0f);
			p.Velocity = Math::Vector3(std::cos(angle), Rand(0.2f, 0.6f), std::sin(angle)) * Rand(2.0f, 5.0f);
			p.Life = Rand(0.5f, 0.9f);
			p.SizeStart = Rand(0.8f, 1.4f);
			p.SizeEnd = Rand(2.2f, 3.2f);
			p.ColorStart = { 0.35f, 0.33f, 0.32f, 0.45f };
			p.ColorEnd = { 0.3f, 0.3f, 0.3f, 0.0f };
			p.Drag = 2.5f;
			p.Shape = ParticleShape::Smoke;
			p.Additive = false;
			p.RotationSpeed = Rand(-1.0f, 1.0f);
			Spawn(p);
		}
	}

	void BreakBurst(const Math::Vector3& Position)
	{
		const Color gold = { 1.0f, 0.75f, 0.3f, 1.0f };
		Flash(Position, Scale(gold, 18.0f, 1.0f), 3.5f, 0.25f);
		Ring(Position, Scale(gold, 8.0f, 1.0f), 0.5f, 9.0f, 0.5f, false);
		Ring(Position - Math::Vector3(0, Position.y, 0), Scale(gold, 6.0f, 1.0f), 1.0f, 14.0f, 0.6f, true);
		Sparks(Position, { 0, 1, 0 }, 1.5f, 60, Scale(gold, 10.0f, 1.0f), 10.0f, 40.0f, 0.45f, 0.6f);
	}

	void DeathBurst(const Math::Vector3& Position, const Graphics::Color& Tint)
	{
		Flash(Position, Scale(Tint, 14.0f, 1.0f), 4.0f, 0.35f);
		Ring(Position - Math::Vector3(0, Position.y, 0), Scale(Tint, 8.0f, 1.0f), 1.0f, 20.0f, 0.8f, true);
		for (int i = 0; i < 90; i++)
		{
			Particle p;
			p.Position = Position + RandomUnit() * Rand(0.0f, 2.5f);
			p.Velocity = Math::Vector3(Rand(-1.5f, 1.5f), Rand(3.0f, 9.0f), Rand(-1.5f, 1.5f));
			p.Life = Rand(1.0f, 2.2f);
			p.SizeStart = Rand(0.15f, 0.35f);
			p.SizeEnd = 0.0f;
			p.ColorStart = Scale(Tint, Rand(6.0f, 14.0f), 1.0f);
			p.ColorEnd = Scale(Tint, 2.0f, 0.0f);
			p.Drag = 0.6f;
			p.Gravity = -2.0f;
			p.Shape = ParticleShape::SoftDot;
			Spawn(p);
		}
	}

	void SkillWind(const Math::Vector3& Position, float Radius)
	{
		const Color wind = { 0.55f, 1.0f, 0.8f, 1.0f };
		Ring(Position + Math::Vector3(0, 0.2f, 0), Scale(wind, 6.0f, 1.0f), 1.0f, Radius, 0.5f, true);
		Ring(Position + Math::Vector3(0, 0.2f, 0), Scale(wind, 4.0f, 0.8f), 0.5f, Radius * 0.7f, 0.7f, true);
		for (int i = 0; i < 160; i++)
		{
			Particle p;
			const float angle = Rand(0.0f, 6.2831853f);
			const float r = Rand(0.5f, Radius * 0.6f);
			const float h = Rand(0.0f, 9.0f);
			p.Position = Position + Math::Vector3(std::cos(angle) * r, h, std::sin(angle) * r);
			//	接線方向 + 上昇で渦を作る
			const Math::Vector3 tangent(-std::sin(angle), 0.0f, std::cos(angle));
			p.Velocity = tangent * Rand(14.0f, 26.0f) + Math::Vector3(0, Rand(4.0f, 10.0f), 0);
			p.Life = Rand(0.5f, 1.0f);
			p.SizeStart = Rand(0.25f, 0.5f);
			p.SizeEnd = 0.0f;
			p.ColorStart = Scale(wind, Rand(4.0f, 8.0f), 1.0f);
			p.ColorEnd = Scale(wind, 1.0f, 0.0f);
			p.Drag = 1.2f;
			p.Stretch = 0.04f;
			p.Shape = ParticleShape::Spark;
			Spawn(p);
		}
		for (int i = 0; i < 18; i++)
		{
			Particle p;
			const float angle = Rand(0.0f, 6.2831853f);
			p.Position = Position + Math::Vector3(std::cos(angle), 0.3f, std::sin(angle)) * Rand(0.5f, Radius * 0.4f);
			p.Velocity = Math::Vector3(std::cos(angle), 0.4f, std::sin(angle)) * Rand(6.0f, 12.0f);
			p.Life = Rand(0.8f, 1.2f);
			p.SizeStart = 1.5f;
			p.SizeEnd = 4.0f;
			p.ColorStart = { 0.45f, 0.5f, 0.48f, 0.35f };
			p.ColorEnd = { 0.4f, 0.4f, 0.4f, 0.0f };
			p.Drag = 2.0f;
			p.Shape = ParticleShape::Smoke;
			p.Additive = false;
			Spawn(p);
		}
	}

	void SkillFire(const Math::Vector3& Position, float Radius)
	{
		const Color fire = { 1.0f, 0.42f, 0.12f, 1.0f };
		Flash(Position + Math::Vector3(0, 1.5f, 0), Scale(fire, 14.0f, 1.0f), 3.5f, 0.25f);
		Ring(Position + Math::Vector3(0, 0.2f, 0), Scale(fire, 8.0f, 1.0f), 1.0f, Radius, 0.45f, true);
		for (int i = 0; i < 140; i++)
		{
			Particle p;
			const float angle = Rand(0.0f, 6.2831853f);
			const float r = Rand(0.0f, Radius * 0.5f);
			p.Position = Position + Math::Vector3(std::cos(angle) * r, Rand(0.0f, 1.0f), std::sin(angle) * r);
			p.Velocity = Math::Vector3(Rand(-2.0f, 2.0f), Rand(6.0f, 18.0f), Rand(-2.0f, 2.0f));
			p.Life = Rand(0.4f, 0.9f);
			p.SizeStart = Rand(1.0f, 2.0f);
			p.SizeEnd = 0.2f;
			p.ColorStart = Scale(fire, Rand(6.0f, 12.0f), 1.0f);
			p.ColorEnd = { 0.6f, 0.08f, 0.02f, 0.0f };
			p.Drag = 1.0f;
			p.Gravity = -4.0f;
			p.Shape = ParticleShape::SoftDot;
			p.RotationSpeed = Rand(-2.0f, 2.0f);
			Spawn(p);
		}
		Sparks(Position + Math::Vector3(0, 1.0f, 0), { 0, 1, 0 }, 1.0f, 50, Scale(fire, 12.0f, 1.0f), 10.0f, 30.0f, 0.25f, 1.2f);
	}

	void GuardAura(const Math::Vector3& Position)
	{
		if (Rand(0.0f, 1.0f) > 0.5f)
		{
			return;
		}
		Particle p;
		const float angle = Rand(0.0f, 6.2831853f);
		p.Position = Position + Math::Vector3(std::cos(angle) * 1.6f, Rand(0.5f, 4.0f), std::sin(angle) * 1.6f);
		p.Velocity = Math::Vector3(0, Rand(1.0f, 2.5f), 0);
		p.Life = 0.4f;
		p.SizeStart = 0.18f;
		p.SizeEnd = 0.0f;
		p.ColorStart = { 1.5f, 3.0f, 6.0f, 1.0f };
		p.ColorEnd = { 0.5f, 1.0f, 2.0f, 0.0f };
		p.Shape = ParticleShape::SoftDot;
		Spawn(p);
	}

	void StunSparkle(const Math::Vector3& HeadPosition)
	{
		if (Rand(0.0f, 1.0f) > 0.35f)
		{
			return;
		}
		Particle p;
		const float angle = Rand(0.0f, 6.2831853f);
		p.Position = HeadPosition + Math::Vector3(std::cos(angle) * 1.2f, Rand(0.0f, 0.6f), std::sin(angle) * 1.2f);
		p.Velocity = Math::Vector3(-std::sin(angle), 0.0f, std::cos(angle)) * 3.0f;
		p.Life = 0.5f;
		p.SizeStart = 0.6f;
		p.SizeEnd = 0.1f;
		p.ColorStart = { 12.0f, 9.0f, 3.0f, 1.0f };
		p.ColorEnd = { 4.0f, 2.0f, 0.5f, 0.0f };
		p.Shape = ParticleShape::Flare;
		p.RotationSpeed = 4.0f;
		Spawn(p);
	}
}
