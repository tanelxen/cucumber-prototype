class WeaponMP40 : MonoBehaviour
{
    float fireInterval = 0.1f;
    int boneIndex = 5;

    private AnimatorComponent@ anim;
    private bool isFiring = false;
    private float fireTimer = 0.0f;

    private bool isReloading = false;

    private int flashTextureId = 0;
    private int bulletHoleTexId = 0;
    int damage = 10;

    int ammo = 0;
    int clip = 32;
    private int kMaxClip = 32;

    private int iconTextureId = 0;

    void start()
    {
        print("WeaponMP40 started on entity: " + GetOwner().GetName());

        @anim = GetOwner().GetAnimator();
        @anim.onClipFinished = OnClipFinishedCallback(this.handleAnimationEnd);

        anim.Play("idle", false, 0.0f);

        flashTextureId = SpriteRenderer::preloadTexture("assets/textures/muzzle_flash.png");
        bulletHoleTexId = SpriteRenderer::preloadTexture("assets/textures/bullet_hole.png");

        iconTextureId = SpriteRenderer::preloadTexture("assets/textures/iconw_MP40_1.png");
    }

    void update(float dt)
    {
        if (canFire())
        {
            if (Input::GetLeftMouseButtonPressed())
            {
                startFiring();
            }
            else
            {
                stopFiring();
            }

            if (fireTimer > 0.0f) {
                fireTimer -= dt;
            }

            // Логика стрельбы работает строго по таймеру, пока зажата кнопка
            if (isFiring && fireTimer <= 0.0f) {
                executeLogicShot();
            }
        }

        if (Input::GetKeyDown(KeyCode::R))
        {
            reload();
        }

        drawAmmo();
    }

    void handleAnimationEnd(const string &in clipName)
    {
        if (clipName == "reload")
        {
            int needed = kMaxClip - clip;
            int remain = Math::max(0, ammo - needed);
            int added = ammo - remain;

            clip += added;
            ammo = remain;

            anim.Play("idle", false); // Возвращаемся в idle
            isReloading = false;
        }
    }

    // Игрок нажал ЛКМ
    void startFiring()
    {
        if (isFiring) return;
        isFiring = true;

        // ВИЗУАЛ: Запускаем анимацию выстрела в ЗАЦИКЛЕННОМ режиме (once = false)
        // Оружие начинает непрерывно воспроизводить цикл отдачи/работы затвора
        anim.Play("attack", false, 0.1f); 
        
        // Первый выстрел делаем мгновенно, без задержки
        executeLogicShot();
    }

    // Игрок отпустил ЛКМ
    void stopFiring()
    {
        if (!isFiring) return;
        isFiring = false;

        // ВИЗУАЛ: Переключаемся обратно на зацикленный idle
        anim.Play("idle", false, 0.2f);
    }

    void executeLogicShot()
    {
        clip -= 1;

        fireTimer = fireInterval; // Сброс таймера

        PlaySound("assets/sounds/mp40f1.wav");

        Vector3 muzzlePos = anim.getBoneWorldPosition(boneIndex);

        float randomRot = Math::RandomFloat(0.0f, 6.28f);
        float randomScale = Math::RandomFloat(12.0f, 16.0f);

        SpriteRenderer::AddBillboard(flashTextureId, muzzlePos, randomScale, randomRot);
        SpawnSmoke(muzzlePos, 1);

        
        TransformComponent@ transform = GetTransform();
        Vector3 position = transform.GetWorldPosition();
        Vector3 forward = transform.GetRight();
        RaycastResult hit;

        if (Physics::Raycast(position, forward, 1024.0f, hit, GetOwner())) 
        {
            SpawnTracer(muzzlePos, hit.point);

            if (hit.entity != null)
            {
                ref@ zom = _GetScript(hit.entity, "Zombie");
                Zombie@ zombie = cast<Zombie>(zom);

                if (zombie != null)
                {
                    zombie.takeDamage(damage);
                }

                SpawnBlood(hit.point, hit.normal, 5);
            }
            else
            {
                SpawnSparks(hit.point, hit.normal, 5);
                SpawnDust(hit.point, 5);

                float randomScale = Math::RandomFloat(4.0f, 8.0f);
                DecalManager::AddDecal(bulletHoleTexId, hit.point, hit.normal, randomScale, 60.0f);
            }
        }
    }

    private void reload()
    {
        if (ammo > 0 && clip < kMaxClip)
        {
            PlaySound("assets/sounds/mp40_reload.wav");
            anim.Play("reload", true);
            isReloading = true;
        }
    }

    private bool canFire()
    {
        return clip > 0 && !isReloading;
    }

    private void drawAmmo()
    {
        /*
        UI::DrawText(
            UI::LayoutBottomRight(144, 16, 0, 0),
            "" + clip,
            Color(0.63f, 0.59f, 0.57f, 1.0f)
        );

        UI::DrawImage(
            UI::LayoutBottomRight(16, 0, 128, 64),
            iconTextureId,
            Color(1.0f, 1.0f, 1.0f, 1.0f)
        );

        UI::DrawText(
            UI::LayoutBottomRight(16, 64, 0, 0),
            "" + ammo,
            Color(0.63f, 0.59f, 0.57f, 1.0f)
        );
        */
    }
}