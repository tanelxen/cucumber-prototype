class WeaponMP40 : MonoBehaviour
{
    float fireInterval = 0.1f;

    private AnimatorComponent@ anim;
    private bool isFiring = false;
    private float fireTimer = 0.0f;

    void start()
    {
        @anim = GetOwner().GetAnimator();

        anim.Play("idle", false);
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
        anim.Play("attack", false); 
        
        // Первый выстрел делаем мгновенно, без задержки
        executeLogicShot();
    }

    // Игрок отпустил ЛКМ
    void stopFiring()
    {
        if (!isFiring) return;
        isFiring = false;

        // ВИЗУАЛ: Переключаемся обратно на зацикленный idle
        anim.Play("idle", false);
    }

    void executeLogicShot()
    {
        fireTimer = fireInterval; // Сброс таймера

        // Чисто геймплейные действия: тратим патрон, считаем попадания
        //print("Логический выстрел! Наносим урон.");
        PlaySound("assets/sounds/mp40f1.wav");
    }
}