class WeaponMP40 : MonoBehaviour
{
    float fireInterval = 0.1f;
    int boneIndex = 5;

    private AnimatorComponent@ anim;
    private bool isFiring = false;
    private float fireTimer = 0.0f;

    private int flashTextureId = 0;

    void start()
    {
        print("WeaponMP40 started on entity: " + GetOwner().GetName());

        @anim = GetOwner().GetAnimator();

        anim.Play("idle", false, 0.0f);

        flashTextureId = SpriteRenderer::preloadTexture("assets/textures/muzzle_flash.png");
    }

    void update(float dt)
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
        fireTimer = fireInterval; // Сброс таймера

        // Чисто геймплейные действия: тратим патрон, считаем попадания
        //print("Логический выстрел! Наносим урон.");
        PlaySound("assets/sounds/mp40f1.wav");

        //TransformComponent@ transform = GetTransform();
        //Vector3 position = transform.GetWorldPosition();
        //Vector3 forward = transform.GetRight();
        //Vector3 muzzleWorldPos = position + forward * 32.0f;

        Vector3 muzzlePos = anim.getBoneWorldPosition(boneIndex);

        float randomRot = Math::RandomFloat(0.0f, 6.28f);
        float randomScale = Math::RandomFloat(12.0f, 16.0f);

        SpriteRenderer::addBillboard(flashTextureId, muzzlePos, randomScale, randomRot);
    }
}