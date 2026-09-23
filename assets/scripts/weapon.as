class Weapon : MonoBehaviour
{
    [Range(1, 20), Tooltip("Выстрелов в сек")]
    int fireRate = 1;
    int ammo = 0;

    string effect = "Пыщ-Пыщ";

    void start()
    {
        //print("Weapon spawned on entity: " + GetOwner().GetName());

        AnimatorComponent@ animator = GetOwner().GetAnimator();

        if (animator != null)
        {
            // Подписываемся на завершение анимации
            @animator.onClipFinished = OnClipFinishedCallback(this.handleAnimationEnd);

            animator.addNotify("reload", 0.0f, OnAnimNotify(this.PlayReloadSound));
            animator.addNotify("attack", 0.0f, OnAnimNotify(this.PlayShotSound));
        }
    }

    void update(float dt)
    {
        if (Input::GetLeftMouseButtonClicked())
        {
            Fire();
        }

        if (Input::GetKeyDown(KeyCode::R))
        {
            Reload();
        }
    }

    void PlayReloadSound()
    {
        //print("Звук перезарядки");
        PlaySound("assets/sounds/mp40_reload.wav");
    }

    void PlayShotSound()
    {
        //print("Звук выстрела");
        PlaySound("assets/sounds/mp40f1.wav");
    }

    void Fire()
    {
        AnimatorComponent@ animator = GetOwner().GetAnimator();

        if (animator != null)
        {
            //animator.Play(1, true);
            animator.PlayQueue("attack", "idle", false);
        }
    }

    void Reload()
    {
        //print("Reload");
        AnimatorComponent@ animator = GetOwner().GetAnimator();

        if (animator != null)
        {
            //animator.Play(4, true);
            //animator.PlayQueue("reload", "idle", false);

            // Подписываемся на события таймлайна анимации
            @animator.onAnimEvent = function(const string &in eventName, const string &in eventParam) {
                print(eventName);

                if (eventName == "play_sound") {
                    // Вызываем твою аудио-систему miniaudio
                    // eventParam содержит путь к файлу: "assets/audio/mag_click.wav"
                    //AudioSystem::play(eventParam); 
                }
                if (eventName == "spawn_fx") {
                    // Например, выброс гильзы или партикл
                    //FXSystem::spawn(eventParam, this.getMuzzlePosition());
                }
            };

            animator.Play("reload", true);
            ammo = 0;

            /*
            // Передаем ЛЯМБДУ (анонимную функцию) прямо в коллбэк!
            @animator.onClipFinished = function(const string &in clipName) {
                if (clipName == "reload") {
                    print("Лямбда: Перезарядка завершена!");
                }
            };
            */
        }
    }

    void handleAnimationEnd(const string &in clipName)
    {
        if (clipName == "reload")
        {
            this.ammo = 30; // Выдаем патроны ТОЛЬКО когда анимация завершилась!

            AnimatorComponent@ animator = GetOwner().GetAnimator();
            animator.Play("idle", false); // Возвращаемся в idle

            @animator.onClipFinished = null; // Отписываемся
        }
    }
}